from pydantic import BaseModel, ConfigDict
from typing import Dict, Any

class SongCreate(BaseModel):
    name: str
    tsn_tsn: str

class MusicianUpdate(BaseModel):
    uzepatscher: str
    instrument_register: str
    color: str
    position: str

class SongImport(BaseModel):
    name: str
    tsn_content: str
