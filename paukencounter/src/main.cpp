// =============================================================================
// Pauke Monitor - ESP32 Timpani Hit Counter
// =============================================================================
// Hardware: ESP32 DevKit + B39 Piezo Vibrationssensor an GPIO 34 (ADC1)
// Funktion: Zaehlt Schlaege auf einer 49-Zoll Pauke, loggt Daten,
//           stellt einen Webserver mit Live-Statistiken bereit.
// =============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

// -----------------------------------------------------------------------------
// Konfiguration
// -----------------------------------------------------------------------------

// Sensor
static const int PIEZO_PIN = 34;
static const int SCHWELLENWERT = 300;       // ADC-Schwelle fuer Schlagerkennung
static const int COOLDOWN_MS = 80;          // Debounce-Zeit in ms
static const int ADC_MAX = 4095;            // 12-bit ADC Maximum

// WiFi Access Point
static const char* AP_SSID = "Pauke-Monitor";
static const char* AP_PASS = "timpani123";

// Dateispeicher
static const char* CSV_PATH = "/schlaege.csv";

// -----------------------------------------------------------------------------
// Globale Variablen
// -----------------------------------------------------------------------------

AsyncWebServer server(80);

unsigned long session_start = 0;
unsigned long last_hit_time = 0;
uint32_t schlag_nummer = 0;
double gesamt_energie = 0.0;

// -----------------------------------------------------------------------------
// Hilfsfunktionen
// -----------------------------------------------------------------------------

// Berechnet Energie in Prozent aus ADC-Rohwert
float berechne_energie(int intensitaet) {
    return (float)intensitaet / (float)ADC_MAX * 100.0f;
}

// Formatiert Sekunden als HH:MM:SS
String format_zeit(unsigned long sekunden) {
    unsigned long h = sekunden / 3600;
    unsigned long m = (sekunden % 3600) / 60;
    unsigned long s = sekunden % 60;
    char buf[12];
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", h, m, s);
    return String(buf);
}

// Formatiert Sekunden als MM:SS fuer Log-Eintraege
String format_log_zeit(unsigned long sekunden) {
    unsigned long m = sekunden / 60;
    unsigned long s = sekunden % 60;
    char buf[8];
    snprintf(buf, sizeof(buf), "%lu:%02lu", m, s);
    return String(buf);
}

// -----------------------------------------------------------------------------
// LittleFS: CSV lesen / schreiben / loeschen
// -----------------------------------------------------------------------------

// Liest bestehende Logs und stellt Zaehler wieder her
void lade_bestehende_daten() {
    if (!LittleFS.exists(CSV_PATH)) {
        schlag_nummer = 0;
        gesamt_energie = 0.0;
        return;
    }

    File f = LittleFS.open(CSV_PATH, "r");
    if (!f) {
        schlag_nummer = 0;
        gesamt_energie = 0.0;
        return;
    }

    schlag_nummer = 0;
    gesamt_energie = 0.0;

    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        // Format: Nummer;Zeitstempel;Intensitaet;Energie%
        int sep1 = line.indexOf(';');
        int sep2 = line.indexOf(';', sep1 + 1);
        int sep3 = line.indexOf(';', sep2 + 1);

        if (sep1 < 0 || sep2 < 0 || sep3 < 0) continue;

        uint32_t nr = line.substring(0, sep1).toInt();
        float energie = line.substring(sep3 + 1).toFloat();

        if (nr > schlag_nummer) {
            schlag_nummer = nr;
        }
        gesamt_energie += energie;
    }
    f.close();

    Serial.printf("Bestehende Daten geladen: %u Schlaege, Energie: %.1f%%\n",
                  schlag_nummer, gesamt_energie);
}

// Speichert einen einzelnen Schlag in die CSV
void speichere_schlag(uint32_t nummer, unsigned long zeitstempel_s, int intensitaet, float energie) {
    File f = LittleFS.open(CSV_PATH, "a");
    if (!f) {
        Serial.println("FEHLER: CSV konnte nicht geoeffnet werden!");
        return;
    }
    f.printf("%u;%lu;%d;%.1f\n", nummer, zeitstempel_s, intensitaet, energie);
    f.close();
}

// Loescht alle Log-Daten
void loesche_logs() {
    if (LittleFS.exists(CSV_PATH)) {
        LittleFS.remove(CSV_PATH);
    }
    schlag_nummer = 0;
    gesamt_energie = 0.0;
    session_start = millis();
    Serial.println("Logs geloescht.");
}

// Liest die letzten N Zeilen der CSV
String lese_letzte_zeilen(int anzahl) {
    if (!LittleFS.exists(CSV_PATH)) return "";

    File f = LittleFS.open(CSV_PATH, "r");
    if (!f) return "";

    // Alle Zeilen einlesen
    std::vector<String> zeilen;
    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
            zeilen.push_back(line);
        }
    }
    f.close();

    // Nur die letzten N zurueckgeben (neueste zuerst)
    String result = "";
    int start = zeilen.size() > (size_t)anzahl ? zeilen.size() - anzahl : 0;
    for (int i = zeilen.size() - 1; i >= start; i--) {
        result += zeilen[i] + "\n";
    }
    return result;
}

// Liest gesamte CSV
String lese_gesamte_csv() {
    if (!LittleFS.exists(CSV_PATH)) return "Nummer;Zeitstempel;Intensitaet;Energie%\n";

    File f = LittleFS.open(CSV_PATH, "r");
    if (!f) return "";

    String content = "Nummer;Zeitstempel;Intensitaet;Energie%\n";
    while (f.available()) {
        content += f.readStringUntil('\n') + "\n";
    }
    f.close();
    return content;
}

// -----------------------------------------------------------------------------
// HTML-Seite (inline, keine externen Abhaengigkeiten)
// -----------------------------------------------------------------------------

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
<title>Pauke Monitor</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:'Courier New',monospace;background:#1a1a2e;color:#e0e0e0;
  min-height:100vh;display:flex;flex-direction:column;align-items:center}
.container{width:100%;max-width:480px;padding:12px}
.header{text-align:center;padding:16px 0;border-bottom:2px solid #444}
.title{font-size:1.4em;color:#f0a500;font-weight:bold;letter-spacing:2px}
.stats{display:flex;justify-content:space-around;padding:20px 0}
.stat-box{text-align:center}
.stat-label{font-size:0.8em;color:#888;text-transform:uppercase;letter-spacing:1px}
.stat-value{font-size:2.8em;color:#fff;font-weight:bold;line-height:1.1}
.stat-value.energie{color:#f0a500}
.bar-container{width:100%;height:28px;background:#333;border-radius:4px;
  margin:8px 0;overflow:hidden;position:relative}
.bar-fill{height:100%;background:linear-gradient(90deg,#2d6a4f,#52b788,#f0a500);
  border-radius:4px;transition:width 0.5s ease}
.bar-text{position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);
  font-size:0.85em;font-weight:bold;color:#fff;text-shadow:1px 1px 2px #000}
.session{text-align:center;color:#888;font-size:0.9em;padding:4px 0 12px;
  border-bottom:2px solid #444}
.buttons{display:flex;gap:8px;padding:12px 0;flex-wrap:wrap;justify-content:center}
.btn{padding:10px 16px;border:1px solid #555;background:#2a2a3e;color:#e0e0e0;
  border-radius:6px;font-family:inherit;font-size:0.85em;cursor:pointer;
  flex:1;min-width:100px;text-align:center}
.btn:active{background:#444}
.btn.danger{border-color:#c0392b;color:#e74c3c}
.btn.danger:active{background:#c0392b;color:#fff}
.log-table-wrap{overflow-x:auto;margin-top:8px;max-height:50vh;overflow-y:auto;
  border:1px solid #333;border-radius:6px}
table{width:100%;border-collapse:collapse;font-size:0.8em}
th{background:#2a2a3e;color:#f0a500;padding:8px 6px;text-align:left;
  position:sticky;top:0;z-index:1}
td{padding:6px;border-bottom:1px solid #2a2a3e}
tr:hover{background:#2a2a3e}
.csv-area{display:none;margin-top:10px}
.csv-area textarea{width:100%;height:200px;background:#111;color:#0f0;
  border:1px solid #444;border-radius:4px;padding:8px;font-family:monospace;
  font-size:0.75em}
.flash{animation:flashAnim 0.4s ease}
@keyframes flashAnim{0%{background:#f0a500}100%{background:transparent}}
</style>
</head>
<body>
<div class="container">
  <div class="header">
    <div class="title">&#x1F941; PAUKE MONITOR (49")</div>
  </div>

  <div class="stats">
    <div class="stat-box">
      <div class="stat-label">Schl&auml;ge</div>
      <div class="stat-value" id="count">--</div>
    </div>
    <div class="stat-box">
      <div class="stat-label">Energie</div>
      <div class="stat-value energie" id="energy">--</div>
    </div>
  </div>

  <div class="bar-container">
    <div class="bar-fill" id="bar" style="width:0%"></div>
    <div class="bar-text" id="barText">0%</div>
  </div>

  <div class="session">Session: <span id="session">00:00:00</span></div>

  <div class="buttons">
    <button class="btn" onclick="loadData()">Aktualisieren</button>
    <button class="btn danger" onclick="confirmDelete()">L&ouml;schen</button>
    <button class="btn" onclick="showCSV()">CSV kopieren</button>
  </div>

  <div class="csv-area" id="csvArea">
    <textarea id="csvText" readonly></textarea>
    <button class="btn" onclick="copyCSV()" style="margin-top:6px;width:100%">In Zwischenablage kopieren</button>
  </div>

  <div class="log-table-wrap">
    <table>
      <thead>
        <tr><th>#</th><th>Zeit</th><th>Intensit&auml;t</th><th>Energie</th></tr>
      </thead>
      <tbody id="logBody">
        <tr><td colspan="4" style="text-align:center;color:#666">Lade...</td></tr>
      </tbody>
    </table>
  </div>
</div>

<script>
var autoRefresh = null;

function loadData() {
  fetch('/api/stats')
    .then(function(r){return r.json()})
    .then(function(d){
      document.getElementById('count').textContent = d.schlaege;
      document.getElementById('energy').textContent = d.gesamt_energie.toFixed(0);
      var pct = d.schlaege > 0 ? Math.min(100, d.gesamt_energie / d.schlaege) : 0;
      document.getElementById('bar').style.width = pct.toFixed(1) + '%';
      document.getElementById('barText').textContent = pct.toFixed(0) + '%';
      document.getElementById('session').textContent = d.session_dauer;

      var body = document.getElementById('logBody');
      if (!d.letzte_logs || d.letzte_logs.length === 0) {
        body.innerHTML = '<tr><td colspan="4" style="text-align:center;color:#666">Keine Daten</td></tr>';
        return;
      }
      var html = '';
      for (var i = 0; i < d.letzte_logs.length; i++) {
        var e = d.letzte_logs[i];
        html += '<tr><td>' + e.nr + '</td><td>' + e.zeit + '</td><td>' +
                e.intensitaet + '</td><td>' + e.energie.toFixed(1) + '%</td></tr>';
      }
      body.innerHTML = html;
    })
    .catch(function(err){console.log('Fehler:', err)});
}

function confirmDelete() {
  if (confirm('Alle Logs wirklich loeschen?')) {
    fetch('/api/delete', {method:'POST'})
      .then(function(){loadData()});
  }
}

function showCSV() {
  var area = document.getElementById('csvArea');
  if (area.style.display === 'block') {
    area.style.display = 'none';
    return;
  }
  fetch('/api/logs')
    .then(function(r){return r.text()})
    .then(function(t){
      document.getElementById('csvText').value = t;
      area.style.display = 'block';
    });
}

function copyCSV() {
  var ta = document.getElementById('csvText');
  ta.select();
  ta.setSelectionRange(0, 99999);
  document.execCommand('copy');
  alert('CSV kopiert!');
}

loadData();
autoRefresh = setInterval(loadData, 3000);
</script>
</body>
</html>
)rawliteral";

// -----------------------------------------------------------------------------
// Webserver-Routen
// -----------------------------------------------------------------------------

void setup_webserver() {
    // Hauptseite
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", INDEX_HTML);
    });

    // API: Statistiken als JSON
    server.on("/api/stats", HTTP_GET, [](AsyncWebServerRequest *request) {
        unsigned long session_s = (millis() - session_start) / 1000;

        // Letzte 50 Eintraege lesen
        String letzte = lese_letzte_zeilen(50);

        // JSON zusammenbauen
        String json = "{";
        json += "\"schlaege\":" + String(schlag_nummer) + ",";
        json += "\"gesamt_energie\":" + String(gesamt_energie, 1) + ",";
        json += "\"session_dauer\":\"" + format_zeit(session_s) + "\",";
        json += "\"letzte_logs\":[";

        // Zeilen parsen
        int pos = 0;
        bool first = true;
        while (pos < (int)letzte.length()) {
            int nl = letzte.indexOf('\n', pos);
            if (nl < 0) nl = letzte.length();
            String line = letzte.substring(pos, nl);
            line.trim();
            pos = nl + 1;

            if (line.length() == 0) continue;

            int s1 = line.indexOf(';');
            int s2 = line.indexOf(';', s1 + 1);
            int s3 = line.indexOf(';', s2 + 1);
            if (s1 < 0 || s2 < 0 || s3 < 0) continue;

            String nr = line.substring(0, s1);
            String ts = line.substring(s1 + 1, s2);
            String intensitaet = line.substring(s2 + 1, s3);
            String energie = line.substring(s3 + 1);

            unsigned long ts_val = ts.toInt();
            String zeit_fmt = format_log_zeit(ts_val);

            if (!first) json += ",";
            json += "{\"nr\":" + nr + ",\"zeit\":\"" + zeit_fmt + "\",";
            json += "\"intensitaet\":" + intensitaet + ",";
            json += "\"energie\":" + energie + "}";
            first = false;
        }

        json += "]}";
        request->send(200, "application/json", json);
    });

    // API: Gesamte CSV herunterladen
    server.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest *request) {
        String csv = lese_gesamte_csv();
        AsyncWebServerResponse *response = request->beginResponse(200, "text/csv", csv);
        response->addHeader("Content-Disposition", "attachment; filename=schlaege.csv");
        request->send(response);
    });

    // API: Logs loeschen
    server.on("/api/delete", HTTP_POST, [](AsyncWebServerRequest *request) {
        loesche_logs();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });

    server.begin();
    Serial.println("Webserver gestartet auf http://192.168.4.1");
}

// -----------------------------------------------------------------------------
// WiFi Access Point
// -----------------------------------------------------------------------------

void setup_wifi() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    delay(100);

    IPAddress ip = WiFi.softAPIP();
    Serial.printf("WiFi AP gestartet: SSID='%s', IP=%s\n", AP_SSID, ip.toString().c_str());
}

// -----------------------------------------------------------------------------
// Serial-Abfrage beim Start: Logs loeschen oder fortsetzen?
// -----------------------------------------------------------------------------

void serial_startup_prompt() {
    if (!LittleFS.exists(CSV_PATH)) {
        Serial.println("Keine bestehenden Logs gefunden. Starte neu.");
        return;
    }

    Serial.println("========================================");
    Serial.println("Bestehende Logs gefunden!");
    Serial.printf("  %u Schlaege, Gesamtenergie: %.1f%%\n", schlag_nummer, gesamt_energie);
    Serial.println("----------------------------------------");
    Serial.println("  [L] Logs loeschen");
    Serial.println("  [beliebige Taste / 5s warten] Fortsetzen");
    Serial.println("========================================");

    unsigned long start = millis();
    while (millis() - start < 5000) {
        if (Serial.available()) {
            char c = Serial.read();
            if (c == 'L' || c == 'l') {
                loesche_logs();
                Serial.println(">> Logs geloescht. Starte mit leeren Daten.");
                return;
            } else {
                Serial.println(">> Fortsetzen mit bestehenden Daten.");
                return;
            }
        }
        delay(50);
    }
    Serial.println(">> Timeout - Fortsetzen mit bestehenden Daten.");
}

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("========================================");
    Serial.println("   PAUKE MONITOR - 49\" Timpani");
    Serial.println("========================================");

    // ADC konfigurieren
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    pinMode(PIEZO_PIN, INPUT);

    // LittleFS initialisieren
    if (!LittleFS.begin(true)) {
        Serial.println("FEHLER: LittleFS konnte nicht initialisiert werden!");
        return;
    }
    Serial.println("LittleFS initialisiert.");

    // Bestehende Daten laden
    lade_bestehende_daten();

    // Serial-Abfrage
    serial_startup_prompt();

    // Session-Start merken
    session_start = millis();

    // WiFi und Webserver starten
    setup_wifi();
    setup_webserver();

    Serial.println("========================================");
    Serial.println("Bereit! Warte auf Schlaege...");
    Serial.println("========================================");
}

// -----------------------------------------------------------------------------
// Hauptschleife: Schlag-Erkennung
// -----------------------------------------------------------------------------

void loop() {
    int raw = analogRead(PIEZO_PIN);

    if (raw >= SCHWELLENWERT) {
        unsigned long jetzt = millis();

        // Cooldown pruefen (Debouncing)
        if (jetzt - last_hit_time < COOLDOWN_MS) {
            return;
        }

        // Peak-Erkennung: Kurze Zeit abtasten um Maximum zu finden
        int peak = raw;
        unsigned long peak_start = millis();
        while (millis() - peak_start < 10) {
            int sample = analogRead(PIEZO_PIN);
            if (sample > peak) {
                peak = sample;
            }
        }

        last_hit_time = millis();
        schlag_nummer++;

        unsigned long zeitstempel_s = (millis() - session_start) / 1000;
        float energie = berechne_energie(peak);
        gesamt_energie += energie;

        // In CSV speichern
        speichere_schlag(schlag_nummer, zeitstempel_s, peak, energie);

        // Serial-Ausgabe
        Serial.printf("SCHLAG #%u | Zeit: %s | Intensitaet: %d | Energie: %.1f%% | Gesamt: %.0f%%\n",
                      schlag_nummer,
                      format_log_zeit(zeitstempel_s).c_str(),
                      peak,
                      energie,
                      gesamt_energie);
    }
}
