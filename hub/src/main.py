import asyncio
import uvicorn
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from src.hub_api.routes import router as hub_router
from src.nano_network.api import router as nano_router
from src.nano_network.serial_gateway import SerialGateway
from src.nano_network.nano_manager import NanoManager
from src.nano_network.hotspot import check_hotspot_status
from src.config import settings
from src.show.player_instance import player as song_player
from fastapi.staticfiles import StaticFiles
from fastapi.responses import FileResponse
import os

app = FastAPI()

app.mount("/static", StaticFiles(directory=settings.STATIC_DIR), name="static")

app.add_middleware(
	CORSMiddleware,
	allow_origins=["*"],
	allow_credentials=True,
	allow_methods=["*"],
	allow_headers=["*"],
)

app.include_router(hub_router)
app.include_router(nano_router)

gateway = SerialGateway()
nano_manager = NanoManager()


@app.on_event("startup")
async def startup_event():
	"""Initialize serial gateway and start heartbeat on server startup."""
	# Hotspot is managed by hostapd systemd service, just check status
	hotspot_ok = await check_hotspot_status()
	if hotspot_ok:
		print("Hotspot (hostapd) is running")
	else:
		print("Warning: Hotspot (hostapd) is not running!")

	if gateway.connect():
		await gateway.start_heartbeat_loop()
		await gateway.start_read_loop()
		print("Serial gateway initialized and heartbeat started")
	else:
		print("Warning: Serial gateway not connected - no device found")


@app.on_event("shutdown")
async def shutdown_event():
	"""Clean up on server shutdown."""
	gateway.stop_heartbeat_loop()
	gateway.disconnect()
	# Hotspot is managed by hostapd, don't stop it
	print("Serial gateway shut down")


@app.get("/gateway/status")
async def get_gateway_status():
	"""Get serial gateway connection status."""
	return {
		"connected": gateway.is_connected,
		"heartbeat_active": gateway._heartbeat_task is not None
	}


@app.post("/gateway/reconnect")
async def reconnect_gateway():
	"""Attempt to reconnect the serial gateway."""
	if gateway.connect():
		if not gateway._heartbeat_task:
			await gateway.start_heartbeat_loop()
		return {"status": "connected"}

	raise HTTPException(status_code=503, detail="No serial device found")


@app.post("/songs/next")
async def next_part():
	if song_player.is_playing:
		success = song_player.jump_to_next_part()
		return {"success": success}
	return {"success": False, "error": "No song is currently playing"}


@app.get("/{page_name:path}")
async def serve_pages(page_name: str):
	if page_name.startswith(("api/", "static/")):
		raise HTTPException(status_code=404, detail="Not found")

	if page_name == "":
		page_name = "index.html"
	elif not page_name.endswith(".html"):
		page_name = f"{page_name}.html"

	file_path = os.path.join(settings.PAGES_DIR, page_name)

	if settings.DEBUG:
		print(f"Serving: {file_path}")

	if not os.path.isfile(file_path):
		raise HTTPException(status_code=404, detail="Page not found")

	return FileResponse(file_path)


if __name__ == "__main__":
	uvicorn.run(
		app,
		host=settings.HOST_IP,
		port=settings.API_PORT,
		log_level="info"
	)
