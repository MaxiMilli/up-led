
# Nano aktualiseren

1. mit SSH verbinden

```bash
ssh uzi@nano.local
```

```bash
nanohub
```

2. in den Ordner `nano` wechseln

```bash
cd /home/uzi/uzepatscher-led-gwaendli
```

3. Git pull

```bash
git pull
```

4. Service neu starten

```bash
sudo systemctl restart uzi.service
```

## Dig deeper

1. Service status

```bash
sudo systemctl status uzi.service
```

2. Logs anzeigen

```bash
sudo journalctl -u uzi.service -f
```

3. Service Datei bearbeiten

```bash
sudo nano /etc/systemd/system/uzi.service
```
