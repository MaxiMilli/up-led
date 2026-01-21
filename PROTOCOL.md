# Protocol Master Document

Dieses Dokument definiert die verbindlichen Protokoll-Konstanten fuer das gesamte Monorepo.
Alle Komponenten (Hub, Gateway, Nano, Applausmaschine) muessen diese Werte konsistent verwenden.

---

## Frame-Groessen

| Konstante             | Wert | Beschreibung                        |
| --------------------- | ---- | ----------------------------------- |
| SERIAL_FRAME_SIZE     | 18   | Serial Frame: START + PAYLOAD + CHK |
| ESPNOW_PAYLOAD_SIZE   | 16   | ESP-NOW Payload (ohne START/CHK)    |
| SERIAL_START_BYTE     | 0xAA | Start-Byte fuer Downstream          |
| SERIAL_START_UPSTREAM | 0xBB | Start-Byte fuer Upstream            |

### Quellen-Vergleich

| Komponente      | Frame Size                | Payload Size | Status |
| --------------- | ------------------------- | ------------ | ------ |
| Hub             | 18                        | 16           | OK     |
| Gateway         | 18                        | 16           | OK     |
| Nano            | 18 (Serial), 16 (ESP-NOW) | 16           | OK     |
| Applausmaschine | 16 (nur ESP-NOW)          | 16           | OK     |

---

## Gruppen (Bitmask)

Gruppen werden als 16-bit Bitmask uebertragen. Jeder Nano kann mehreren Gruppen angehoeren.

### Instrument-Gruppen (1-7)

| Register | Gruppe       | Bitmask | Hex    | Instrument   |
| -------- | ------------ | ------- | ------ | ------------ |
| -        | kAll         | bit 0   | 0x0001 | Alle Nanos   |
| 1        | kDrums       | bit 1   | 0x0002 | Drums        |
| 2        | kPauken      | bit 2   | 0x0004 | Pauken       |
| 3        | kTschinellen | bit 3   | 0x0008 | Tschinellen  |
| 4        | kLiras       | bit 4   | 0x0010 | Liras        |
| 5        | kTrompeten   | bit 5   | 0x0020 | Trompeten    |
| 6        | kPosaunen    | bit 6   | 0x0040 | Posaunen     |
| 7        | kBaesse      | bit 7   | 0x0080 | Baesse       |
| -        | kBroadcast   | alle    | 0xFFFF | Broadcast    |

### Reservierte Gruppen (8-15)

| Gruppe     | Bitmask  | Hex    | Status     |
| ---------- | -------- | ------ | ---------- |
| kGroup8    | bit 8    | 0x0100 | Reserviert |
| kGroup9    | bit 9    | 0x0200 | Reserviert |
| kGroup10   | bit 10   | 0x0400 | Reserviert |
| kGroup11   | bit 11   | 0x0800 | Reserviert |
| kGroup12   | bit 12   | 0x1000 | Reserviert |
| kGroup13   | bit 13   | 0x2000 | Reserviert |
| kGroup14   | bit 14   | 0x4000 | Reserviert |
| kGroup15   | bit 15   | 0x8000 | Reserviert |

### MIDI-Note zu Gruppe Mapping

Das Hub mappt MIDI-Noten aus TSN-Dateien auf die vereinheitlichten Gruppen:

| MIDI-Note | Urspruengliche Bezeichnung | Ziel-Gruppe  |
| --------- | -------------------------- | ------------ |
| 1         | Ganze Gugge                | kAll         |
| 2         | Drums                      | kDrums (1)   |
| 4         | Pauken                     | kPauken (2)  |
| 5         | Lira                       | kLiras (4)   |
| 6         | Chinellen                  | kTschinellen (3) |
| 7         | 1. Trompete                | kTrompeten (5) |
| 8         | 2. Trompete                | kTrompeten (5) |
| 9         | 1. Posaune                 | kPosaunen (6) |
| 10        | 2. Posaune                 | kPosaunen (6) |
| 11        | 3. Posaune                 | kPosaunen (6) |
| 12        | Baesse                     | kBaesse (7)  |
| 13        | Baesse Instrumente         | kBaesse (7)  |
| 14        | Waegeli Instrumente        | kDrums (1)   |
| 15        | Pauken Instrumente         | kPauken (2)  |

### Quellen-Vergleich

| Komponente      | kAll   | Instrumente   | kBroadcast | Status |
| --------------- | ------ | ------------- | ---------- | ------ |
| Hub             | 0x0001 | 0x0002-0x0080 | 0xFFFF     | OK     |
| Nano            | 0x0001 | 0x0002-0x0080 | 0xFFFF     | OK     |
| Applausmaschine | 0x0001 | 0x0002-0x0080 | 0xFFFF     | OK     |

---

## Commands

### System Commands (0x00-0x0F)

| Command       | ID   | Beschreibung                      |
| ------------- | ---- | --------------------------------- |
| kNop          | 0x00 | No Operation                      |
| kHeartbeat    | 0x01 | Heartbeat/Keep-alive              |
| kPing         | 0x02 | Ping Request                      |
| kIdentify     | 0x03 | LED-Identifikation (blinken)      |
| kSetLedCount  | 0x04 | LED-Anzahl setzen                 |
| kSetGroups    | 0x05 | Gruppen-Zugehoerigkeit setzen     |
| kSaveConfig   | 0x06 | Konfiguration in EEPROM speichern |
| kReboot       | 0x07 | Nano neu starten                  |
| kFactoryReset | 0x0A | Werkseinstellungen                |
| kSetMeshTTL   | 0x0B | Mesh TTL setzen                   |

### State Commands (0x10-0x1F)

| Command         | ID   | Beschreibung               |
| --------------- | ---- | -------------------------- |
| kStateOff       | 0x10 | LEDs aus, Schlafmodus      |
| kStateStandby   | 0x11 | Standby-Farbe anzeigen     |
| kStateActive    | 0x12 | Aktiv, bereit fuer Effekte |
| kStateEmergency | 0x13 | Notfall-Modus              |
| kStateBlackout  | 0x14 | Sofort alle LEDs aus       |

### Effect Commands (0x20-0x3F)

| Effect              | ID   | Beschreibung         |
| ------------------- | ---- | -------------------- |
| kEffectSolid        | 0x20 | Einfarbig            |
| kEffectBlink        | 0x21 | Blinken              |
| kEffectFade         | 0x22 | Ein-/Ausblenden      |
| kEffectRainbow      | 0x23 | Regenbogen           |
| kEffectRainbowCycle | 0x24 | Regenbogen-Zyklus    |
| kEffectChase        | 0x25 | Lauflicht            |
| kEffectTheaterChase | 0x26 | Theater-Lauflicht    |
| kEffectTwinkle      | 0x27 | Funkeln              |
| kEffectSparkle      | 0x28 | Glitzern             |
| kEffectFire         | 0x29 | Feuer-Effekt         |
| kEffectPulse        | 0x2A | Pulsieren            |
| kEffectStrobe       | 0x2B | Stroboskop           |
| kEffectGradient     | 0x2C | Farbverlauf          |
| kEffectWave         | 0x2D | Wellen-Effekt        |
| kEffectMeteor       | 0x2E | Meteor/Sternschnuppe |
| kEffectBreathing    | 0x2F | Atmen-Effekt         |

### Pairing Commands (0x80-0xAF)

| Command         | ID   | Beschreibung                   |
| --------------- | ---- | ------------------------------ |
| kPairingAckRecv | 0x81 | Pairing ACK empfangen (intern) |
| kConfigSetRecv  | 0x82 | Config Set empfangen (intern)  |
| kConfigAck      | 0x83 | Config ACK Bestaetigung        |
| kPairingRequest | 0xA0 | Pairing-Anfrage vom Nano       |

### Debug Commands (0xF0-0xFF)

| Command      | ID   | Beschreibung        |
| ------------ | ---- | ------------------- |
| kDebugEcho   | 0xF0 | Echo-Test           |
| kDebugInfo   | 0xF1 | Debug-Informationen |
| kDebugStress | 0xF2 | Stress-Test         |

---

## Flags und TTL

Das Flags-Byte (Byte 2 im Payload) ist wie folgt aufgeteilt:

```
Byte 2: [TTL:4 bits][Flags:4 bits]
        Bits 7-4: TTL (0-15 Hops)
        Bits 3-0: Flags
```

### TTL (Time-to-Live)

| Wert | Beschreibung            |
| ---- | ----------------------- |
| 0    | Nicht rebroadcasten     |
| 1    | Max 1 Hop               |
| 2    | Max 2 Hops (Default)    |
| 3    | Max 3 Hops              |

Bei jedem Rebroadcast wird TTL um 1 dekrementiert. Bei TTL=0 wird nicht mehr rebroadcastet.

### Flags (untere 4 Bits)

| Flag           | Wert | Beschreibung              |
| -------------- | ---- | ------------------------- |
| kPriority      | 0x01 | Hohe Prioritaet           |
| kForce         | 0x02 | Erzwinge Ausfuehrung      |
| kSync          | 0x04 | Synchronisiert            |
| kNoRebroadcast | 0x08 | Nicht weiterleiten (Mesh) |

---

## Checksum

Die Checksum wird mit CRC-8 (Polynom 0x07, Init 0x00) berechnet.

---

## Payload-Format (16 Bytes)

```
Byte  0-1:  Sequence Number (uint16, big-endian)
Byte  2:    TTL (obere 4 Bits) + Flags (untere 4 Bits)
Byte  3:    Command/Effect ID
Byte  4-5:  Groups (uint16, big-endian)
Byte  6-7:  Duration in ms (uint16, big-endian)
Byte  8:    Length (Effekt-Parameter)
Byte  9:    Rainbow (0=aus, 1=an)
Byte  10:   Red (0-255)
Byte  11:   Green (0-255)
Byte  12:   Blue (0-255)
Byte  13-14: Speed in ms (uint16, big-endian)
Byte  15:   Intensity/Brightness (0-255)
```

---

## Hinweise

### Applausmaschine: kSync Flag

Die Applausmaschine definiert `kSync` (0x04) nicht, da sie dieses Flag nicht verwendet.
Hub und Nano haben dieses Flag definiert.

---

## Quell-Dateien

| Komponente      | Datei                                  |
| --------------- | -------------------------------------- |
| Hub (Commands)  | hub/src/nano_network/serial_gateway.py |
| Hub (Effects)   | hub/src/effects/effect_types.py        |
| Gateway         | gateway/include/constants.h            |
| Nano            | nano/include/constants.h               |
| Applausmaschine | applausmaschine/include/constants.h    |
