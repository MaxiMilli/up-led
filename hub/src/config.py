from pydantic_settings import BaseSettings
from typing import List
import os

class Settings(BaseSettings):
    HOST_IP: str = "0.0.0.0"
    DATABASE_URL: str = "sqlite+aiosqlite:///./led_control.db"
    API_PORT: int = 8000
    DEBUG: bool = True
    SONGS_DIR: str = "/home/uzi/uzepatscher-led-gwaendli/hub/songs"
    STATIC_DIR: str = "/home/uzi/uzepatscher-led-gwaendli/hub/static"
    PAGES_DIR: str = "/home/uzi/uzepatscher-led-gwaendli/hub/pages"
    FIRMWARE_DIR: str = "/home/uzi/uzepatscher-led-gwaendli/hub/firmware"

    SERIAL_PORTS: List[str] = ["/dev/ttyUSB0", "/dev/ttyACM0"]
    SERIAL_BAUD: int = 115200
    HEARTBEAT_INTERVAL_MS: int = 5000

    WIFI_SSID: str = "uzepatscher_lichtshow"
    WIFI_PASSWORD: str = "kWalkingLight"

    model_config = {
        "env_file": ".env.test" if "TESTING" in os.environ else ".env",
        "extra": "ignore"
    }

settings = Settings()
