# Raspberry Pi Deployment (Ubuntu)

Anleitung zum Deployen der Hub-Software auf einem Raspberry Pi mit Ubuntu.

## Voraussetzungen

- Raspberry Pi mit Ubuntu (22.04 oder neuer)
- SSH-Zugang zum Pi
- Gateway-ESP32 per USB angeschlossen
- Mac als Entwicklungsrechner

## 1. SSH-Verbindung herstellen

```bash
ssh ubuntu@<PI_IP_ADRESSE>
```

Falls du die IP nicht kennst:
```bash
# Auf dem Mac im gleichen Netzwerk
ping raspberrypi.local
# oder
arp -a | grep -i raspberry
```

## 2. System vorbereiten

```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y python3 python3-pip python3-venv git
```

## 3. Benutzer zur dialout-Gruppe hinzufügen

Für Serial-Port-Zugriff (`/dev/ttyUSB0` oder `/dev/ttyACM0`):

```bash
sudo usermod -aG dialout $USER
```

**Wichtig:** Nach diesem Befehl neu einloggen oder Pi neustarten:
```bash
sudo reboot
```

## 4. Projekt auf den Pi kopieren

### Option A: Mit Git (empfohlen)

```bash
cd ~
git clone <REPO_URL> uzepatscher-led-gwaendli
cd uzepatscher-led-gwaendli/hub
```

### Option B: Mit rsync vom Mac

Auf dem Mac ausführen:
```bash
rsync -avz --exclude '__pycache__' --exclude '.git' --exclude 'venv' \
  /pfad/zu/uzepatscher-led-gwaendli/hub/ \
  ubuntu@<PI_IP_ADRESSE>:~/hub/
```

## 5. Python Virtual Environment einrichten

```bash
cd ~/hub
python3 -m venv venv
source venv/bin/activate
pip install --upgrade pip
pip install -r requirements.txt
```

## 6. Konfiguration anpassen

Die Pfade in `src/config.py` müssen auf den Pi angepasst werden. Erstelle eine `.env` Datei:

```bash
cat > .env << 'EOF'
SONGS_DIR=/home/ubuntu/hub/songs
STATIC_DIR=/home/ubuntu/hub/static
PAGES_DIR=/home/ubuntu/hub/pages
EOF
```

Oder passe die `src/config.py` direkt an:
```bash
nano src/config.py
```

Ändere die Pfade:
```python
SONGS_DIR: str = "/home/ubuntu/hub/songs"
STATIC_DIR: str = "/home/ubuntu/hub/static"
PAGES_DIR: str = "/home/ubuntu/hub/pages"
```

## 7. Serial-Port prüfen

Gateway-ESP32 per USB anschliessen und prüfen:

```bash
ls -la /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

Sollte `/dev/ttyUSB0` oder `/dev/ttyACM0` anzeigen.

## 8. Manueller Test

```bash
cd ~/hub
source venv/bin/activate
python -m src.main
```

Der Server sollte auf Port 8000 starten. Teste im Browser:
```
http://<PI_IP_ADRESSE>:8000
```

Mit `Ctrl+C` beenden.

## 9. Systemd-Service einrichten (Autostart)

Service-Datei erstellen:

```bash
sudo nano /etc/systemd/system/led-hub.service
```

Inhalt einfügen:

```ini
[Unit]
Description=LED Hub Service
After=network.target

[Service]
Type=simple
User=ubuntu
WorkingDirectory=/home/ubuntu/hub
Environment="PATH=/home/ubuntu/hub/venv/bin"
ExecStart=/home/ubuntu/hub/venv/bin/python -m src.main
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

Service aktivieren und starten:

```bash
sudo systemctl daemon-reload
sudo systemctl enable led-hub
sudo systemctl start led-hub
```

## 10. Service verwalten

```bash
# Status prüfen
sudo systemctl status led-hub

# Logs anzeigen
sudo journalctl -u led-hub -f

# Neustarten
sudo systemctl restart led-hub

# Stoppen
sudo systemctl stop led-hub
```

## 11. Updates deployen

### Mit Git

```bash
cd ~/hub
git pull
sudo systemctl restart led-hub
```

### Mit rsync (vom Mac)

```bash
rsync -avz --exclude '__pycache__' --exclude '.git' --exclude 'venv' --exclude '.env' \
  /pfad/zu/uzepatscher-led-gwaendli/hub/ \
  ubuntu@<PI_IP_ADRESSE>:~/hub/

ssh ubuntu@<PI_IP_ADRESSE> "sudo systemctl restart led-hub"
```

## Troubleshooting

### Serial-Port nicht gefunden

```bash
# Prüfen ob Gerät erkannt wird
dmesg | tail -20

# Berechtigungen prüfen
ls -la /dev/ttyUSB0
# Sollte "dialout" Gruppe zeigen

# Prüfen ob User in Gruppe ist
groups
# Sollte "dialout" enthalten
```

### Service startet nicht

```bash
# Detaillierte Logs
sudo journalctl -u led-hub -n 50 --no-pager

# Manuell testen
cd ~/hub
source venv/bin/activate
python -m src.main
```

### Port 8000 bereits belegt

```bash
# Prüfen was den Port nutzt
sudo lsof -i :8000

# Anderen Port in .env setzen
echo "API_PORT=8080" >> .env
```

## Schnell-Deploy-Script (auf dem Mac)

Erstelle `deploy.sh` lokal:

```bash
#!/bin/bash
PI_HOST="ubuntu@<PI_IP_ADRESSE>"
REMOTE_DIR="~/hub"

rsync -avz --exclude '__pycache__' --exclude '.git' --exclude 'venv' --exclude '.env' \
  ./ ${PI_HOST}:${REMOTE_DIR}/

ssh ${PI_HOST} "sudo systemctl restart led-hub"
echo "Deployed and restarted!"
```

Nutzung:
```bash
cd /pfad/zu/hub
chmod +x deploy.sh
./deploy.sh
```
