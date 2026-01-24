import asyncio


async def _run(cmd: list[str]) -> tuple[int, str, str]:
    proc = await asyncio.create_subprocess_exec(
        *cmd,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
    )
    stdout, stderr = await proc.communicate()
    return proc.returncode, stdout.decode().strip(), stderr.decode().strip()


async def start_hotspot(ssid: str, password: str) -> bool:
    """Start a WiFi hotspot using NetworkManager (nmcli)."""
    try:
        # Disconnect existing wifi connection on wlan0
        await _run(["/usr/bin/sudo", "/usr/bin/nmcli", "device", "disconnect", "wlan0"])

        # Delete old hotspot connection if present
        await _run(["/usr/bin/sudo", "/usr/bin/nmcli", "connection", "delete", "Hotspot"])

        # Create and activate hotspot
        rc, stdout, stderr = await _run([
            "/usr/bin/sudo", "/usr/bin/nmcli", "device", "wifi", "hotspot",
            "ifname", "wlan0",
            "ssid", ssid,
            "password", password,
        ])

        if rc == 0:
            print(f"WiFi hotspot '{ssid}' started successfully")
            return True
        else:
            print(f"Failed to start hotspot: {stderr}")
            return False
    except Exception as e:
        print(f"Error starting hotspot: {e}")
        return False


async def stop_hotspot() -> bool:
    """Stop the WiFi hotspot."""
    try:
        rc, stdout, stderr = await _run(["/usr/bin/sudo", "/usr/bin/nmcli", "connection", "down", "Hotspot"])
        if rc == 0:
            print("WiFi hotspot stopped")
            return True
        else:
            print(f"Failed to stop hotspot: {stderr}")
            return False
    except Exception as e:
        print(f"Error stopping hotspot: {e}")
        return False
