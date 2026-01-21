from flask import Flask, request, Response
import subprocess
import os

app = Flask(__name__)

# =========================================
# Routes
# =========================================

@app.route("/sys/logs")
def get_logs():
    """
    Zeigt die letzten Logs von uzimonitor.service.
    Du kannst hier z.B. nur die letzten 100 Zeilen
    oder die letzte Stunde ausgeben lassen.
    """
    # Beispiel: letzte 1000 Zeilen
    cmd = ["journalctl", "-u", "uzi.service", "-n", "1000", "--no-pager"]
    try:
        output = subprocess.check_output(cmd).decode("utf-8")
    except subprocess.CalledProcessError as e:
        output = "Fehler beim Lesen der Logs:\n" + str(e)
    return f"<pre>{output}</pre>"


@app.route("/sys/reboot")
def reboot_pi():
    """
    Startet den Raspberry Pi neu.
    """
    os.system("sudo reboot now")
    return "Raspberry Pi wird neu gestartet...<br><br><a href='http://hub.local:8000/'>Zum Tool</a>"

@app.route("/sys/update")
def update_pi():
    """
    Aktualisiert den Raspberry Pi.
    """
    os.system("cd /home/uzi/uzepatscher-led-gwaendli && git pull && sudo systemctl restart uzi.service")
    return "Raspberry Pi wird neu gestartet...<br><br><a href='http://hub.local:8000/'>Zum Tool</a>"

@app.route("/sys/restart")
def restart_pi():
    """
    Startet den Raspberry Pi neu.
    """
    os.system("sudo systemctl restart uzi.service")
    return "Raspberry Pi wird neu gestartet...<br><br><a href='http://hub.local:8000/'>Zum Tool</a>"


@app.route("/sys/shutdown")
def shutdown_pi():
    """
    Fährt den Raspberry Pi herunter.
    """
    os.system("sudo shutdown -h now")
    return "Raspberry Pi wird heruntergefahren...<br><br><a href='http://hub.local:8000/'>Zum Tool</a>"


if __name__ == "__main__":
    # Starte den Server auf Port 8080 und binde an 0.0.0.0,
    # damit er aus dem lokalen Netzwerk erreichbar ist
    app.run(host="0.0.0.0", port=8080)
