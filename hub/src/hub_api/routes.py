# src/api/routes.py
from fastapi import FastAPI, HTTPException, WebSocket, Body, APIRouter
from fastapi.middleware.cors import CORSMiddleware
import xml.etree.ElementTree as ET
import json
import os
from datetime import datetime, UTC
from sqlalchemy import select
from src.hub_api.models import SongCreate, MusicianUpdate, SongImport
from src.show.command import CommandManager
from src.config import settings
import asyncio
from src.hub_api.song_metadata import SongMetadataManager
from src.show.player_instance import player as song_player
from src.websocket.websocket_manager import websocket_manager
from src.nano_network.nano_manager import NanoManager
from src.nano_network.serial_gateway import SerialGateway, GROUP_BROADCAST
from ..effects.effect_processor import effect_processor, EffectSettings

router = APIRouter()
command_manager = CommandManager()
metadata_manager = SongMetadataManager()
nano_manager = NanoManager()

async def send_blackout():
	"""
	Sends blackout command to all nanos
	"""
	blackout_command = [
		0x14,  # STATE_BLACKOUT
		0, 0,  # duration
		0,     # intensity
		0, 0, 0,  # RGB
		0,     # rainbow
		0, 0,  # speed
		0      # length
	]
	await nano_manager.broadcast_command(blackout_command, target_register=None)


@router.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await websocket_manager.connect(websocket)
    try:
        while True:
            data = await websocket.receive_text()
            try:
                message = json.loads(data)
                await websocket_manager.handle_message(websocket, message)
            except json.JSONDecodeError:
                print(f"Invalid JSON received: {data}")
                pass
    except Exception as e:
        print(f"WebSocket error: {e}")
    finally:
        websocket_manager.disconnect(websocket)
        if len(websocket_manager.active_connections) == 0:
            print("All WebSocket connections closed")

@router.post("/songs/import/{song_name}")
async def import_song(song_name: str, file_content: str = Body(..., media_type="text/plain")):
    try:
        # Create songs directory if it doesn't exist
        songs_dir = os.path.join(os.getcwd(), settings.SONGS_DIR)
        os.makedirs(songs_dir, exist_ok=True)
        
        # Get latest version number from existing files
        existing_files = [f for f in os.listdir(songs_dir) 
            if f.startswith(f"{song_name}_v") and f.endswith(".tsn")]
        
        if existing_files:
            # Extract version numbers and find the highest
            versions = [int(f.split('_v')[1].split('_')[0]) for f in existing_files]
            new_version = max(versions) + 1
        else:
            new_version = 1
        
        # Create filename with timestamp and version
        timestamp = datetime.now(UTC).strftime("%Y%m%d_%H%M%S")
        filename = f"{song_name}_v{new_version}_{timestamp}.tsn"
        file_path = os.path.join(songs_dir, filename)
        
        # Save TSN file
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(file_content)
        
        return {
            "status": "success",
            "song_name": song_name,
            "version": new_version,
            "file_path": file_path
        }
            
    except Exception as e:
        print(f"Error importing song: {str(e)}")
        raise HTTPException(status_code=500, detail=str(e))

@router.get("/songs/{song_name}/versions")
async def get_song_versions(song_name: str):
    try:
        # Get all versions from filesystem
        songs_dir = os.path.join(os.getcwd(), settings.SONGS_DIR)
        song_files = [f for f in os.listdir(songs_dir) 
                     if f.startswith(f"{song_name}_v") and f.endswith(".tsn")]
        
        if not song_files:
            raise HTTPException(
                status_code=404, 
                detail=f"No versions found for song '{song_name}'"
            )
        
        versions = []
        for file_name in song_files:
            # Extract version number from filename (format: songname_v{number}_{timestamp}.tsn)
            version = int(file_name.split('_v')[1].split('_')[0])
            file_path = os.path.join(songs_dir, file_name)
            
            versions.append({
                "version": version,
                "created_at": datetime.fromtimestamp(
                    os.path.getmtime(file_path), 
                    tz=UTC
                ).isoformat(),
                "file_path": file_path
            })
        
        # Sort versions by version number, descending
        versions.sort(key=lambda x: x["version"], reverse=True)
        return versions
        
    except HTTPException:
        raise
    except Exception as e:
        print(f"Error getting song versions: {str(e)}")
        raise HTTPException(status_code=500, detail=str(e))

@router.get("/songs/{song_name}/versions/{version}")
async def get_song_version(song_name: str, version: int):
    try:
        # Get all versions from filesystem
        songs_dir = os.path.join(os.getcwd(), settings.SONGS_DIR)
        song_files = [f for f in os.listdir(songs_dir) 
                     if f.startswith(f"{song_name}_v{version}_") and f.endswith(".tsn")]
        
        if not song_files:
            raise HTTPException(
                status_code=404, 
                detail=f"Version {version} not found for song '{song_name}'"
            )
        
        # There should only be one file matching this pattern
        file_name = song_files[0]
        file_path = os.path.join(songs_dir, file_name)
        
        # Read TSN content
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                tsn_content = f.read()
        except FileNotFoundError:
            raise HTTPException(
                status_code=404,
                detail=f"TSN file not found for version {version}"
            )
                
        return {
            "version": version,
            "created_at": datetime.fromtimestamp(
                os.path.getmtime(file_path), 
                tz=UTC
            ).isoformat(),
            "tsn_content": tsn_content
        }
        
    except HTTPException:
        raise
    except Exception as e:
        print(f"Error getting song version: {str(e)}")
        raise HTTPException(status_code=500, detail=str(e))

# Helper function to get song details (to avoid code duplication)
def get_song_details(current_song):
    return {
        "name": current_song["songMetadata"]["name"],
        "tempo": current_song["musicProperties"]["masterTempo"],
        "beatStyle": current_song["musicProperties"]["beatStyle"],
        "track_count": len(current_song["tracks"]),
        # Add tempo changes to response
        "tempoChanges": [
            {
                "tick": tc["tick"],
                "tempo": tc["tempo"]
            }
            for tc in current_song.get("tempoChanges", [])
        ],
        "tracks": [
            {
                "name": track["name"],
                "channel": track["midiChannel"],
                "parts_count": len(track.get("parts", [])),
                "parts": [
                    {
                        "start": {
                            "tick": part["timing"]["start"],
                            "seconds": part["timing"]["start"] * (60.0 / current_song["musicProperties"]["masterTempo"] / 480)
                        },
                        "end": {
                            "tick": part["timing"]["end"],
                            "seconds": part["timing"]["end"] * (60.0 / current_song["musicProperties"]["masterTempo"] / 480)
                        },
                        "notes_count": len(part.get("notes", [])),
                        "part_name": part.get("name", "")
                    }
                    for part in track.get("parts", [])
                ]
            }
            for track in current_song["tracks"] if track["name"] == "LED" or track["name"] == "UP LED"
        ]
    }

@router.get("/songs/current")
async def get_current_song():
    """Get information about the currently loaded song"""
    if not song_player.current_song:
        raise HTTPException(status_code=404, detail="No song currently loaded")

    try:
        return {
            "status": "success",
            "details": get_song_details(song_player.current_song)
        }
    except Exception as e:
        print(f"Error getting current song info: {str(e)}")
        raise HTTPException(status_code=500, detail=str(e))

@router.post("/songs/{song_name}/load")
async def load_song(song_name: str):
    try:
        # Get all versions from filesystem
        songs_dir = os.path.join(os.getcwd(), settings.SONGS_DIR)
        song_files = [f for f in os.listdir(songs_dir) 
                     if f.startswith(f"{song_name}_v") and f.endswith(".tsn")]
        
        if not song_files:
            raise HTTPException(
                status_code=404, 
                detail=f"No versions found for song '{song_name}'"
            )
        
        # Sort files by version number (extract v{number} from filename)
        song_files.sort(key=lambda x: int(x.split('_v')[1].split('_')[0]), reverse=True)
        latest_file = song_files[0]
        version = int(latest_file.split('_v')[1].split('_')[0])
        
        # Read TSN content
        file_path = os.path.join(songs_dir, latest_file)
        print(f"Loading TSN file: {file_path}")
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                tsn_content = f.read()
        except FileNotFoundError:
            raise HTTPException(
                status_code=500,
                detail=f"TSN file not found: {file_path}"
            )

        # Load into song player
        load_result = await song_player.load_song(tsn_content)
        
        if not load_result:
            raise HTTPException(
                status_code=400, 
                detail="Failed to parse song data"
            )

        # Get song information only if load was successful
        current_song = song_player.current_song
        if not current_song:
            raise HTTPException(
                status_code=500,
                detail="Song loaded but data is not available"
            )

        # Blackout-Loop wird jetzt im song_player.load_song() gestartet

        return {
            "status": "success",
            "message": "Song loaded and ready",
            "version": version,
            "details": get_song_details(current_song)
        }

    except HTTPException:
        raise
    except Exception as e:
        print(f"Error loading song: {str(e)}")
        raise HTTPException(status_code=500, detail=str(e))

@router.post("/songs/play")
async def play_song():
    if not song_player.current_song:
        raise HTTPException(status_code=400, detail="No song loaded")

    if song_player.is_playing:
        raise HTTPException(status_code=400, detail="A song is already playing")

    try:
        asyncio.create_task(song_player.play(command_manager))
        
        return {
            "status": "success", 
            "message": "Playback started",
            "song": song_player.current_song["songMetadata"]["name"]
        }
    except Exception as e:
        print(f"Error starting song playback: {str(e)}")
        raise HTTPException(status_code=500, detail=str(e))

@router.delete("/songs/{song_name}")
async def archive_song(song_name: str):
    """Archive (soft delete) a song by renaming its files with 'archived_' prefix"""
    try:
        songs_dir = os.path.join(os.getcwd(), settings.SONGS_DIR)
        song_files = [f for f in os.listdir(songs_dir) 
                     if f.startswith(f"{song_name}_v") and f.endswith(".tsn")]
        
        if not song_files:
            raise HTTPException(
                status_code=404, 
                detail=f"No versions found for song '{song_name}'"
            )
        
        # Rename all versions of the song
        for file_name in song_files:
            old_path = os.path.join(songs_dir, file_name)
            new_name = f"archived_{file_name}"
            new_path = os.path.join(songs_dir, new_name)
            os.rename(old_path, new_path)
        
        return {
            "status": "success",
            "message": f"Song '{song_name}' has been archived",
            "archived_files": len(song_files)
        }
        
    except HTTPException:
        raise
    except Exception as e:
        print(f"Error archiving song: {str(e)}")
        raise HTTPException(status_code=500, detail=str(e))

@router.post("/songs/{song_name}/unarchive")
async def unarchive_song(song_name: str):
    """Unarchive a song by removing the 'archived_' prefix from its files"""
    try:
        songs_dir = os.path.join(os.getcwd(), settings.SONGS_DIR)
        archived_files = [f for f in os.listdir(songs_dir) 
                        if f.startswith(f"archived_{song_name}_v") and f.endswith(".tsn")]
        
        if not archived_files:
            raise HTTPException(
                status_code=404, 
                detail=f"No archived versions found for song '{song_name}'"
            )
        
        # Rename all versions of the song
        for file_name in archived_files:
            old_path = os.path.join(songs_dir, file_name)
            new_name = file_name[9:]  # Remove 'archived_' prefix
            new_path = os.path.join(songs_dir, new_name)
            os.rename(old_path, new_path)
        
        return {
            "status": "success",
            "message": f"Song '{song_name}' has been unarchived",
            "unarchived_files": len(archived_files)
        }
        
    except HTTPException:
        raise
    except Exception as e:
        print(f"Error unarchiving song: {str(e)}")
        raise HTTPException(status_code=500, detail=str(e))

@router.get("/songs")
async def list_songs(archived: bool = False):
    try:
        songs_dir = os.path.join(os.getcwd(), settings.SONGS_DIR)
        song_files = [f for f in os.listdir(songs_dir) if f.endswith(".tsn")]
        
        song_names = set()
        songs_info = []
        
        for file_name in song_files:
            # Skip archived songs unless specifically requested
            is_archived = file_name.startswith("archived_")
            if archived != is_archived:
                continue
                
            # Remove 'archived_' prefix if present
            clean_name = file_name[9:] if is_archived else file_name
            song_name = clean_name.split('_v')[0]
            
            if song_name not in song_names:
                song_names.add(song_name)
                
                # Get all versions (including archived status)
                versions = [f for f in song_files if 
                          (f.startswith(f"archived_{song_name}_v") if is_archived 
                           else f.startswith(f"{song_name}_v"))]
                latest_version = max(versions, key=lambda x: int(x.split('_v')[1].split('_')[0]))
                
                file_path = os.path.join(songs_dir, latest_version)
                try:
                    with open(file_path, 'r', encoding='utf-8') as f:
                        content = f.read()
                        tempo = None
                        beat_style = None
                        
                        for line in content.split('\n'):
                            if 'MasterTempo=' in line:
                                tempo = int(line.split('=')[1].strip("'"))
                            elif 'BeatStyle=' in line:
                                beat_style = line.split('=')[1].strip("'")
                            
                            if tempo and beat_style:
                                break
                        
                        # Get metadata for the song
                        metadata = metadata_manager.get_song_metadata(song_name)
                        
                        songs_info.append({
                            "name": song_name,
                            "song_name": file_path,
                            "versions_count": len(versions),
                            "latest_version": int(latest_version.split('_v')[1].split('_')[0]),
                            "tempo": tempo,
                            "beat_style": beat_style,
                            "label": metadata.get("label", ""),  # Label from metadata
                            "order": metadata.get("order", 0),  # Order from metadata
                            "last_modified": datetime.fromtimestamp(
                                os.path.getmtime(file_path), 
                                tz=UTC
                            ).isoformat()
                        })
                except Exception as e:
                    print(f"Error reading TSN file {file_path}: {e}")
                    continue
        
        return {
            "songs": sorted(songs_info, key=lambda x: x["name"])
        }
        
    except Exception as e:
        print(f"Error listing songs: {str(e)}")
        raise HTTPException(status_code=500, detail=str(e))

@router.put("/songs/{song_name}/label")
async def update_song_label(song_name: str, label: str = Body(...)):
    try:
        metadata_manager.update_song_metadata(song_name, label)
        return {"status": "success", "message": f"Label for {song_name} updated"}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@router.post("/songs/stop")
async def stop_song():
    """Stop the currently playing song"""
    try:
        await song_player.stop()

        song_player.current_song = None
        song_player.is_playing = False
        
        # Send blackout command to all nanos (target_register=None = GROUP_BROADCAST)
        blackout_command = [
            0x14,  # STATE_BLACKOUT
            0, 0,  # duration
            0,     # intensity
            0, 0, 0,  # RGB
            0,     # rainbow
            0, 0,  # speed
            0      # length
        ]
        await nano_manager.broadcast_command(blackout_command, target_register=None)
        
        return {
            "status": "success",
            "message": "Playback stopped and all systems reset"
        }
    except Exception as e:
        print(f"Error stopping song playback: {str(e)}")
        raise HTTPException(status_code=500, detail=str(e))

@router.post("/command")
async def send_command(command: dict = Body(...)):
    """
    Send a command directly to all connected nanos via serial gateway

    Supports all effect codes (0x10-0x3A) and state commands
    """
    try:
        gateway = SerialGateway()

        effect = command.get('effect', 0)
        r = command.get('red', 0)
        g = command.get('green', 0)
        b = command.get('blue', 0)
        intensity = command.get('intensity', 255)
        speed = command.get('speed', 100)
        length = command.get('length', 4)
        rainbow = command.get('rainbow', 0)

        start_time = datetime.now()

        success = gateway.send_command(
            effect=effect,
            groups=GROUP_BROADCAST,
            r=r,
            g=g,
            b=b,
            intensity=intensity,
            speed=speed,
            length=length,
            rainbow=rainbow
        )

        end_time = datetime.now()
        duration = (end_time - start_time).total_seconds()
        print(f"Command processing time: {duration:.6f} seconds")

        if not success:
            raise HTTPException(status_code=503, detail="Failed to send command via serial")

        return {
            "status": "success",
            "message": f"Command 0x{effect:02X} sent via serial gateway",
            "command": command
        }

    except HTTPException:
        raise
    except Exception as e:
        print(f"Error sending command: {str(e)}")
        raise HTTPException(status_code=500, detail=str(e))

@router.put("/songs/order")
async def update_songs_order(song_orders: dict):
    """Update the order of songs"""
    metadata_manager = SongMetadataManager()
    metadata_manager.update_song_order(song_orders)
    return {"status": "success"}
