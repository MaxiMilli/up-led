
# Lokale Installation

0. gehe in den Ordner `/hub` (`cd /hub`)

1. Erstelle und aktiviere eine virtuelle Umgebung:
```bash
python -m venv venv
source venv/bin/activate  # On Mac/Linux
.\venv\Scripts\activate  # On Windows
```

1. Installiere Abhängigkeiten:
```bash
pip install -r requirements.txt
```

1. Kopiere die Umgebungsvariablen:
```bash
cp .env.example .env
```

1. Starte den Entwicklungsserver:
```bash
python -m src.main
```
