# LED Hub

This project implements the central hub component for the Uzepatscher LED show control system. It runs on a Raspberry Pi and manages communication between the tablet interface and LED-equipped Nanos.

```bash
rsync -avz --exclude '__pycache__' --exclude '.git' --exclude 'venv' --exclude 'nano_info.json' \
  ./ \
  pi@192.168.1.195:~/uzepatscher-led-gwaendli/hub/
ssh pi@192.168.1.195 "sudo systemctl restart led-hub"
```


## Setup Instructions

### Prerequisites

- Python 3.9+
- pip
- virtualenv or conda
- SQLite


## Dokumentationen

Es gibt folgende Dokumentationen:

- Swagger API Docs unter [http://127.0.0.1:8000/docs](http://127.0.0.1:8000/docs)
- [Lokale Installation](docs/local_dev_setup.md)
- [Einen Song abspielen](docs/play_song.md)

## Test

1. The server should start and listen for:
   - HTTP requests on port 8000
   - TCP connections on port 9000
   - UDP packets on port 9001 (to be implemented)

2. You can test the API with curl or Postman:
```bash
curl -X POST http://localhost:8000/tablet/songs \
  -H "Content-Type: application/json" \
  -d '{"tsn_xml": "<song><name>Test</name></song>","name":"Test"}'
```

### Test Songs import

1. Song importieren
```bash
curl --location 'http://localhost:8000/songs/import/[name]' \
--header 'Content-Type: text/plain' \
--data '
[TSN-File]
'
```

2. Song Versionen fetchen
curl http://localhost:8000/songs/Testsong/versions

3. Spezifische Song-Version abrufen
curl http://localhost:8000/songs/Testsong/versions/1

3. Letzte Song-Version abrufen
curl http://localhost:8000/songs/Testsong
