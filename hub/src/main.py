import asyncio
import uvicorn
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from src.hub_api.routes import router as hub_router
from src.nano_network.api import router as nano_router
from src.nano_network.tcp_server import TCPServer
from src.nano_network.nano_manager import NanoManager
from src.config import settings
from src.show.player_instance import player as song_player
from fastapi.staticfiles import StaticFiles
from fastapi.responses import FileResponse
import os
import threading

# Create FastAPI app
app = FastAPI()

# Mount static files directory
app.mount("/static", StaticFiles(directory=settings.STATIC_DIR), name="static")

# Enable CORS
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Include routers
app.include_router(hub_router)
app.include_router(nano_router)

async def run_tcp_server():
    # Get NanoManager instance
    nano_manager = NanoManager()
    
    # Create and start TCP server with NanoManager's handler
    tcp_server = TCPServer(client_connected_handler=nano_manager.handle_client_connected)
    
    # Start TCP server
    await tcp_server.start_server(settings.HOST_IP, settings.TCP_PORT)

def run_fastapi():
    uvicorn.run(
        app,
        host=settings.HOST_IP,
        port=settings.API_PORT,
        log_level="info"
    )

@app.post("/songs/next")
async def next_part():
    if song_player.is_playing:
        success = song_player.jump_to_next_part()
        return {"success": success}
    return {"success": False, "error": "No song is currently playing"}

# Serve HTML files from /pages directory
@app.get("/{page_name:path}")
async def serve_pages(page_name: str):
    # Exclude API routes and static files
    if page_name.startswith(("api/", "static/")):
        raise HTTPException(status_code=404, detail="Not found")
    
    # Default to index.html if no path is specified
    if page_name == "":
        page_name = f"index.html"
    # Add .html extension if not present
    elif not page_name.endswith('.html'):
        page_name = f"{page_name}.html"
    
    file_path = os.path.join(settings.PAGES_DIR, page_name)
    
    # Debug print
    print(f"Attempting to serve: {file_path}")
    print(f"File exists: {os.path.isfile(file_path)}")
    print(f"Current working directory: {os.getcwd()}")
    
    # Check if file exists
    if not os.path.isfile(file_path):
        raise HTTPException(status_code=404, detail="Page not found")
        
    return FileResponse(file_path)

if __name__ == "__main__":
    # Start TCP server in a separate thread
    tcp_thread = threading.Thread(
        target=lambda: asyncio.run(run_tcp_server())
    )
    tcp_thread.daemon = True
    tcp_thread.start()
    
    # Run FastAPI in main thread
    run_fastapi()
