from pathlib import Path
import json
import os
from typing import Dict, Optional

class SongMetadataManager:
    def __init__(self):
        self.metadata_file = os.path.join(os.getcwd(), "songs", "metadata.json")
        self._ensure_metadata_file()
        self._ensure_order_field()
    
    def _ensure_metadata_file(self):
        """Erstellt die metadata.json falls sie nicht existiert"""
        if not os.path.exists(self.metadata_file):
            os.makedirs(os.path.dirname(self.metadata_file), exist_ok=True)
            self._save_metadata({})
    
    def _load_metadata(self) -> Dict:
        """Lädt die Metadaten aus der JSON-Datei"""
        try:
            with open(self.metadata_file, 'r', encoding='utf-8') as f:
                return json.load(f)
        except FileNotFoundError:
            return {}
    
    def _save_metadata(self, metadata: Dict):
        """Speichert die Metadaten in die JSON-Datei"""
        with open(self.metadata_file, 'w', encoding='utf-8') as f:
            json.dump(metadata, f, indent=2)
    
    def get_song_metadata(self, song_name: str) -> Dict:
        """Holt die Metadaten für einen bestimmten Song"""
        metadata = self._load_metadata()
        return metadata.get(song_name, {"label": ""})
    
    def update_song_metadata(self, song_name: str, label: str):
        """Aktualisiert die Metadaten für einen Song"""
        metadata = self._load_metadata()
        if song_name not in metadata:
            metadata[song_name] = {}
        metadata[song_name]["label"] = label
        self._save_metadata(metadata)
    
    def _ensure_order_field(self):
        """Ensures all songs have an order field"""
        metadata = self._load_metadata()
        needs_update = False
        max_order = 0
        
        # First pass: find max existing order
        for song_data in metadata.values():
            if "order" in song_data:
                max_order = max(max_order, song_data["order"])
        
        # Second pass: add missing order fields
        for song_name, song_data in metadata.items():
            if "order" not in song_data:
                max_order += 1
                song_data["order"] = max_order
                needs_update = True
        
        if needs_update:
            self._save_metadata(metadata)
    
    def update_song_order(self, song_orders: dict):
        """Updates the order of multiple songs at once"""
        metadata = self._load_metadata()
        for song_name, order in song_orders.items():
            if song_name in metadata:
                metadata[song_name]["order"] = order
        self._save_metadata(metadata) 
