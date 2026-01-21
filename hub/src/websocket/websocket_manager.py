from typing import List
from fastapi import WebSocket

class WebSocketManager:
    def __init__(self):
        self.active_connections: List[WebSocket] = []

    async def connect(self, websocket: WebSocket):
        print("WebSocket connected")
        await websocket.accept()
        self.active_connections.append(websocket)
        
        # Wir holen song_player erst beim Senden
        from src.show.player_instance import player as song_player
        
        # Send initial playback status
        await websocket.send_json({
            "type": "playback_status",
            "is_playing": song_player.is_playing,
            "current_song": song_player.current_song["songMetadata"]["name"] if song_player.current_song else None
        })

    def disconnect(self, websocket: WebSocket):
        self.active_connections.remove(websocket)

    async def broadcast_message(self, message: dict):
        # print(f"Broadcasting message: {message}")
        # print(f"Active connections: {self.active_connections}")
        for connection in self.active_connections:
            try:
                await connection.send_json(message)
            except:
                # If sending fails, we'll handle the cleanup on the connection handler
                pass

    async def handle_message(self, websocket: WebSocket, message: dict):
        """Handle incoming WebSocket messages"""
        try:
            message_type = message.get("type")
            
            # Import song_player here to avoid circular imports
            from src.show.player_instance import player as song_player
            
            if message_type == "next":
                if song_player.current_song and song_player.is_playing:
                    success = await song_player.jump_to_next_part()
                    await self.broadcast_message({
                        "type": "next_response",
                        "success": success,
                        "current_tick": song_player.current_tick
                    })
                else:
                    await self.broadcast_message({
                        "type": "next_response",
                        "success": False,
                        "error": "No song is playing"
                    })
                
            elif message_type == "jump_to_tick":
                if song_player.current_song and song_player.is_playing:
                    tick = message.get("tick")
                    if tick is not None:
                        await song_player.jump_to_tick(tick)
                        await self.broadcast_message({
                            "type": "jump_response",
                            "success": True,
                            "current_tick": song_player.current_tick
                        })
                    else:
                        await self.broadcast_message({
                            "type": "jump_response",
                            "success": False,
                            "error": "No tick specified"
                        })
                else:
                    await self.broadcast_message({
                        "type": "jump_response",
                        "success": False,
                        "error": "No song is playing"
                    })
            
            elif message_type == "hold":
                if song_player.current_song and song_player.is_playing:
                    await song_player.hold()
                    await self.broadcast_message({
                        "type": "hold_response",
                        "success": True,
                        "current_tick": song_player.current_tick
                    })
                else:
                    await self.broadcast_message({
                        "type": "hold_response",
                        "success": False,
                        "error": "No song is playing"
                    })

        except Exception as e:
            print(f"Error handling WebSocket message: {e}")
            await self.broadcast_message({
                "type": "error",
                "message": str(e)
            })

# Create a singleton instance
websocket_manager = WebSocketManager() 
