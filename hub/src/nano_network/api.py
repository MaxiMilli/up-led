from fastapi import APIRouter, HTTPException, UploadFile, File
from fastapi.responses import FileResponse
from pydantic import BaseModel
from typing import Optional
from datetime import datetime
from pathlib import Path
from .nano_manager import NanoManager
from ..config import settings

router = APIRouter()
nano_manager = NanoManager._instance or NanoManager()


class ConfigureNanoRequest(BaseModel):
	register: int
	led_count: int
	name: Optional[str] = None
	standby_r: int = 0
	standby_g: int = 0
	standby_b: int = 255


@router.get("/nano/status")
async def get_nano_status():
	"""Get status of all connected nanos"""
	return nano_manager.get_all_nanos()


@router.post("/nano/update/{mac}")
async def update_nano(mac: str, updates: dict):
	"""Update nano information"""
	success = await nano_manager.update_nano_info(mac, **updates)
	if not success:
		raise HTTPException(status_code=500, detail="Failed to update nano info")
	return {"status": "success"}


@router.post("/nano/pairing/start")
async def start_pairing():
	"""Start pairing mode to accept nano pairing requests"""
	nano_manager.start_pairing_mode()
	return {
		"status": "success",
		"message": "Pairing mode activated. Press button on Nano to pair.",
		"timestamp": datetime.now().isoformat()
	}


@router.post("/nano/pairing/stop")
async def stop_pairing():
	"""Stop pairing mode"""
	nano_manager.stop_pairing_mode()
	return {
		"status": "success",
		"message": "Pairing mode deactivated",
		"timestamp": datetime.now().isoformat()
	}


@router.get("/nano/pairing/status")
async def get_pairing_status():
	"""Get current pairing mode status and pending nano"""
	return {
		"pairing_mode": nano_manager.pairing_mode,
		"pending_mac": nano_manager.pending_pairing_mac,
		"pending_config": nano_manager.nano_info.get(nano_manager.pending_pairing_mac).to_dict()
			if nano_manager.pending_pairing_mac and nano_manager.pending_pairing_mac in nano_manager.nano_info
			else None
	}


@router.post("/nano/configure/{mac}")
async def configure_nano(mac: str, config: ConfigureNanoRequest):
	"""
	Send configuration to a specific Nano.

	This sends the config via ESP-NOW to the Nano which saves it in NVS.
	"""
	success = await nano_manager.configure_nano(
		mac=mac,
		register=config.register,
		led_count=config.led_count,
		name=config.name,
		standby_r=config.standby_r,
		standby_g=config.standby_g,
		standby_b=config.standby_b
	)

	if not success:
		raise HTTPException(status_code=500, detail="Failed to send config to nano")

	return {
		"status": "success",
		"mac": mac,
		"config": {
			"register": config.register,
			"led_count": config.led_count,
			"name": config.name,
			"standby_color": [config.standby_r, config.standby_g, config.standby_b]
		}
	}


@router.delete("/nano/{mac}")
async def remove_nano(mac: str):
	"""Remove a nano from the system"""
	nano_manager.remove_nano(mac)
	return {"status": "success", "mac": mac}


# ============== Firmware OTA Endpoints ==============

@router.get("/firmware/version")
async def get_firmware_version():
	"""Get current firmware version (used by Nanos for OTA check)"""
	version_file = Path(settings.FIRMWARE_DIR) / "version.txt"
	if not version_file.exists():
		return {"version": 0}
	version = int(version_file.read_text().strip())
	return {"version": version}


@router.get("/firmware/binary")
async def get_firmware_binary():
	"""Download firmware binary (used by Nanos for OTA update)"""
	firmware_file = Path(settings.FIRMWARE_DIR) / "firmware.bin"
	if not firmware_file.exists():
		raise HTTPException(status_code=404, detail="No firmware available")
	return FileResponse(firmware_file, media_type="application/octet-stream", filename="firmware.bin")


@router.get("/firmware/info")
async def get_firmware_info():
	"""Get firmware info for Web-UI"""
	firmware_dir = Path(settings.FIRMWARE_DIR)
	version_file = firmware_dir / "version.txt"
	firmware_file = firmware_dir / "firmware.bin"
	return {
		"version": int(version_file.read_text().strip()) if version_file.exists() else 0,
		"has_firmware": firmware_file.exists(),
		"firmware_size": firmware_file.stat().st_size if firmware_file.exists() else 0
	}


@router.post("/firmware/upload")
async def upload_firmware(file: UploadFile = File(...)):
	"""Upload new firmware binary (auto-increments version)"""
	firmware_dir = Path(settings.FIRMWARE_DIR)
	firmware_dir.mkdir(parents=True, exist_ok=True)

	# Save firmware
	firmware_path = firmware_dir / "firmware.bin"
	content = await file.read()
	firmware_path.write_bytes(content)

	# Increment version
	version_file = firmware_dir / "version.txt"
	current = int(version_file.read_text().strip()) if version_file.exists() else 0
	new_version = current + 1
	version_file.write_text(str(new_version))

	return {
		"status": "success",
		"version": new_version,
		"size": len(content),
		"filename": file.filename
	}
