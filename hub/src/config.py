from pydantic_settings import BaseSettings
import os

class Settings(BaseSettings):
    WIFI_SSID: str = "uzepatscher_lichtshow"
    WIFI_PASSWORD: str = "nanohub"
    HOST_IP: str = "0.0.0.0"  # Local development
    DATABASE_URL: str = "sqlite+aiosqlite:///./led_control.db"
    API_PORT: int = 8000
    TCP_PORT: int = 9000
    UDP_PORT: int = 9001
    DEBUG: bool = True
    SONGS_DIR: str = "/home/uzi/uzepatscher-led-gwaendli/hub/songs"  # Ordner für die Song-Files
    STATIC_DIR: str = "/home/uzi/uzepatscher-led-gwaendli/hub/static"
    PAGES_DIR: str = "/home/uzi/uzepatscher-led-gwaendli/hub/pages"

    model_config = {
        "env_file": ".env.test" if "TESTING" in os.environ else ".env"
    }

settings = Settings()
