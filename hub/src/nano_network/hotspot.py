import asyncio


async def _run(cmd: list[str]) -> tuple[int, str, str]:
    proc = await asyncio.create_subprocess_exec(
        *cmd,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
    )
    stdout, stderr = await proc.communicate()
    return proc.returncode, stdout.decode().strip(), stderr.decode().strip()


async def check_hotspot_status() -> bool:
    """Check if hostapd is running (hotspot managed by systemd)."""
    try:
        rc, stdout, stderr = await _run([
            "/usr/bin/systemctl", "is-active", "hostapd"
        ])
        return rc == 0 and stdout == "active"
    except Exception as e:
        print(f"Error checking hotspot status: {e}")
        return False


async def restart_hotspot() -> bool:
    """Restart hostapd service if needed."""
    try:
        rc, stdout, stderr = await _run([
            "/usr/bin/sudo", "/usr/bin/systemctl", "restart", "hostapd"
        ])
        if rc == 0:
            print("Hotspot (hostapd) restarted successfully")
            return True
        else:
            print(f"Failed to restart hotspot: {stderr}")
            return False
    except Exception as e:
        print(f"Error restarting hotspot: {e}")
        return False
