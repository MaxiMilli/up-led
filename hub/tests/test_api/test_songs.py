import pytest
from fastapi.testclient import TestClient
from src.hub_api.routes import app
import os
import shutil
from src.config import settings

# TestClient initialisieren
client = TestClient(app)

# Fixture für temporäres Verzeichnis
@pytest.fixture
def setup_test_directory():
    # Set up a test directory and clean up before and after tests
    test_dir = os.path.join(os.getcwd(), "test_songs")
    if os.path.exists(test_dir):
        shutil.rmtree(test_dir)
    os.makedirs(test_dir, exist_ok=True)
    yield test_dir
    shutil.rmtree(test_dir)

def test_import_song(setup_test_directory):
    # Test-tsn vorbereiten
    test_tsn = """
    <CAS Song>
      <Data>
      SongName='test_song'
      DataVersion=7
      SubVersion=0
      DecVersion=0
      MasterTempo=144
      BeatStyle=BS_16tel
      SwingType=st_8tel
      NotenMin=BS_16tel
      NotenIdeal=BS_8tel
      </Data>
      <!-- Weitere TSN-Daten -->
    </CAS Song>"""
    
    # API-Aufruf
    response = client.post("/songs/import/test_song", headers={"Content-Type": "text/plain"}, content=test_tsn.encode('utf-8'))

    # Überprüfungen
    print(response.json())
    assert response.status_code == 200
    response_data = response.json()
    assert response_data["status"] == "success"
    assert response_data["song_name"] == "test_song"
    
    # Überprüfen, ob die Datei erstellt wurde
    assert os.path.exists(response_data["file_path"])
    
    # Dateiinhalt überprüfen
    with open(response_data["file_path"], 'r', encoding='utf-8') as f:
        saved_content = f.read()
    assert saved_content == test_tsn

def test_import_song_without_led_track(setup_test_directory):
    # Test-TSN ohne LED Track
    test_tsn = """
    <CAS Song>
      <TrackList>
        <TrackData>
          TrackName='RECORD'
        </TrackData>
      </TrackList>
    </CAS Song>"""
    
    # API-Aufruf
    response = client.post("/songs/import/test_song", headers={"Content-Type": "text/plain"}, content=test_tsn.encode('utf-8'))
    
    # Überprüfungen
    assert response.status_code == 200
    response_data = response.json()
    assert response_data["status"] == "success"
    assert response_data["song_name"] == "test_song"
    
    # Überprüfen, ob die Datei erstellt wurde
    assert os.path.exists(response_data["file_path"])
    
    # Dateiinhalt überprüfen
    with open(response_data["file_path"], 'r', encoding='utf-8') as f:
        saved_content = f.read()
    assert saved_content == test_tsn
