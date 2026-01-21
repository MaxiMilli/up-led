from fastapi import APIRouter, HTTPException
from datetime import datetime
from .nano_manager import NanoManager

router = APIRouter()
nano_manager = NanoManager._instance or NanoManager()

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
