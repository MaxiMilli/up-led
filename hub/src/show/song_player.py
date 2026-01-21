import asyncio
import logging
import sys

from dataclasses import dataclass
from typing import Dict, List, Optional
from datetime import datetime

# Externe Services (als Platzhalter importiert):
from ..nano_network.nano_manager import NanoManager
from ..websocket.websocket_manager import websocket_manager
from ..effects.effect_processor import effect_processor, EffectSettings
# from ..effects.coordinator_instance import coordinator
from .tsn_parser import TSNParser

PPQ = 96  # Ticks per Quarter Note (fix in TSN)

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)  # oder INFO in Produktion
log_format = logging.Formatter('%(asctime)s [%(levelname)s] %(message)s')
console_handler = logging.StreamHandler(sys.stdout)
console_handler.setFormatter(log_format)
file_handler = logging.FileHandler("song_player.log", encoding="utf-8")
file_handler.setFormatter(log_format)
logger.addHandler(console_handler)
logger.addHandler(file_handler)

@dataclass
class MidiEvent:
    tick: int
    type: int   # 144 = Note On, 128 = Note Off
    note: int
    velocity: int
    channel: int
    track: int

class SongPlayer:
    def __init__(self):
        self.current_song = None
        self.timeline: List[MidiEvent] = []
        self.events_by_tick: Dict[int, List[MidiEvent]] = {}

        self._player_lock = asyncio.Lock()
        
        # Playback Control
        self.is_playing: bool = False
        self.current_tick: int = 0
        self._max_tick: int = 0
        
        # Jump-Mechanismus
        self._jump_requested: bool = False
        self._jump_target: int = 0
        
        # Part-Timeline
        self.parts_timeline: List[tuple] = []
        
        # Hilfsvariablen
        self.nano_manager = NanoManager._instance or NanoManager()
        self.effect_coordinator = None
        self.channel_0_active = False  # Falls notwendig

        # Task-Referenzen für Playback und Broadcast
        self._playback_task: Optional[asyncio.Task] = None
        self._broadcast_task: Optional[asyncio.Task] = None

        self._is_holding = False

    async def load_song(self, tsn_content: str) -> bool:
        """
        Lädt und parst einen TSN-Song. Baut daraus eine Timeline (Liste) 
        und ein Dictionary events_by_tick. Stellt außerdem Parts-Timeline her.
        """
        async with self._player_lock:
            try:
                logger.info("Debug: Starting song load")
                # (1) Reset
                self.current_song = None
                self.is_playing = False
                self.current_tick = 0
                self.timeline.clear()
                self.events_by_tick.clear()
                self.parts_timeline.clear()
                
                # (2) Parse
                logger.info("Debug: Parsing TSN content")
                parser = TSNParser()
                parsed_data = parser.parse(tsn_content)
                self.current_song = parsed_data
                
                if not parsed_data:
                    logger.debug("Debug: Parser returned None or empty data")
                    return False
                
                # (3) Baue Timeline
                for track_data in parsed_data['tracks']:
                    track_num = track_data['trackNumber']
                    channel = track_data['midiChannel']
                    
                    for part in track_data.get('parts', []):
                        part_start = part['timing']['start']
                        for note_info in part.get('notes', []):
                            if not note_info:
                                continue
                            abs_tick = part_start + note_info['tick']
                            event = MidiEvent(
                                tick=abs_tick,
                                type=note_info['type'],
                                note=note_info['note'],
                                velocity=note_info['velocity'],
                                channel=channel,
                                track=track_num
                            )
                            self.timeline.append(event)

                # (4) Sortieren und gruppieren
                self.timeline.sort(key=lambda e: e.tick)
                self._group_events()
                
                # (5) Parts anlegen (vereinfachtes Beispiel):
                #    -> Wir nehmen alle Part-Starts in einer Liste auf
                for track_data in parsed_data['tracks']:
                    for part in track_data.get('parts', []):
                        start_tick = part['timing']['start']
                        self.parts_timeline.append((start_tick, 0))  # 0 = Platzhalter

                self.parts_timeline.sort(key=lambda x: x[0])
                # Falls wir Ende definieren wollen, zum nächsten Part-Start:
                for i in range(len(self.parts_timeline) - 1):
                    start_current = self.parts_timeline[i][0]
                    start_next = self.parts_timeline[i + 1][0]
                    self.parts_timeline[i] = (start_current, start_next)
                if self.parts_timeline:
                    last_start = self.parts_timeline[-1][0]
                    max_tick = max(e.tick for e in self.timeline) if self.timeline else last_start
                    self.parts_timeline[-1] = (last_start, max_tick)

                # (6) Merke dir max_tick
                if self.timeline:
                    self._max_tick = max(e.tick for e in self.timeline)
                else:
                    self._max_tick = 0

                # (7) Log tempo information
                initial_bpm = self.get_current_tempo(0)
                secs_per_tick = self.get_seconds_per_tick(0)
                # 4 Takte im 3/4 Takt = 12 Beats (Viertel) = 12 * PPQ Ticks
                ticks_in_four_bars = 12 * PPQ
                secs_in_four_bars = secs_per_tick * ticks_in_four_bars
                
                # Berechne Gesamtlänge des Songs
                total_ticks = self._max_tick
                total_seconds = secs_per_tick * total_ticks
                
                logger.info(f"[load_song] Tempo Information:")
                logger.info(f"  - BPM: {initial_bpm}")
                logger.info(f"  - Duration of 4 bars: {secs_in_four_bars:.2f} seconds")
                logger.info(f"  - Total ticks: {total_ticks}")
                logger.info(f"  - Seconds per tick: {secs_per_tick}")
                logger.info(f"  - Calculated total duration: {total_seconds:.2f} seconds")

                            # Reset-Befehl, Falls nötig:
                cmd = bytes([0x64, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0])
                await self.nano_manager.broadcast_command(cmd)

                return True

            except Exception as e:
                logger.error(f"[load_song] Fehler: {e}")
                return False

    def _group_events(self):
        """
        Gruppiert alle MIDI-Events in ein Dict: { tick -> [Liste von Events] }.
        """
        self.events_by_tick.clear()
        for event in self.timeline:
            if event.tick not in self.events_by_tick:
                self.events_by_tick[event.tick] = []
            self.events_by_tick[event.tick].append(event)

    def get_current_tempo(self, tick: int) -> int:
        """
        Ermittelt das gerade gültige Tempo in BPM am angegebenen Tick.
        Berücksichtigt eventuelle Tempo-Änderungen.
        """
        if not self.current_song:
            return 120  # Fallback
        
        current_tempo = self.current_song['musicProperties']['masterTempo']
        tempo_changes = self.current_song.get('tempoChanges', [])
        
        # Wichtig: Nur den letzten gültigen Tempo-Change vor dem aktuellen Tick nehmen
        last_valid_change = None
        for change in sorted(tempo_changes, key=lambda x: x['tick']):
            if change['tick'] <= tick and change['tempo'] > 0:
                last_valid_change = change
            else:
                break
        
        if last_valid_change:
            if current_tempo != last_valid_change['tempo']:
                logger.info(f"[Tempo Change] at tick {tick}: {current_tempo} -> {last_valid_change['tempo']} BPM")
            current_tempo = last_valid_change['tempo']
        
        return current_tempo

    def get_seconds_per_tick(self, tick: int) -> float:
        """
        Rechnet aus, wie viele Sekunden 1 Tick bei dem aktuellen Tempo dauert.
        BPM -> 60 / BPM Sekunden pro Beat (Viertel)
        Bei PPQ=96 Ticks/Beat -> sek_per_tick = (60 / BPM) / 96
        """
        tempo = self.get_current_tempo(tick)
        return 60.0 / (tempo * PPQ)

    async def play(self, command_manager=None, start_tick: int = None) -> bool:
        """
        Startet die Wiedergabe als asynchronen Task.
        """
        async with self._player_lock:
            if not self.current_song or not self.timeline:
                logger.info("[play] Keine Daten vorhanden.")
                return False
            
            if self.is_playing:
                logger.info("[play] Läuft bereits.")
                return False
            
            # Wenn kein expliziter start_tick angegeben wurde,
            # nehmen wir den Start des ersten Parts
            if start_tick is None and self.parts_timeline:
                start_tick = self.parts_timeline[0][0]  # Erster Part-Start
            elif start_tick is None:
                start_tick = 0
            
            logger.info(f"[play] Starte Song Playback ab Tick {start_tick}.")
            self.is_playing = True
            self.current_tick = start_tick
            
            # Haupt-Schleife als Task starten:
            self._playback_task = asyncio.create_task(self._playback_loop(command_manager))

            # Broadcast Task starten (z.B. jede Sekunde)
            self._broadcast_task = asyncio.create_task(self._broadcast_position_loop())

            return True

    async def _playback_loop(self, command_manager):
        """
        Der eigentliche Playback-Loop.
        """
        try:
            start_time = asyncio.get_event_loop().time()
            last_tick_time = start_time
            elapsed_ticks = 0
            current_tempo = self.get_current_tempo(self.current_tick)
            
            while self.is_playing and self.current_tick <= self._max_tick:
                # Check für Hold
                if self._is_holding:
                    await asyncio.sleep(0.1)  # Kurze Pause während Hold
                    continue
                    
                current_time = asyncio.get_event_loop().time()
                
                # (1) Check auf Jump
                if self._jump_requested:
                    self.current_tick = self._jump_target
                    elapsed_ticks = 0
                    self._jump_requested = False
                    current_tempo = self.get_current_tempo(self.current_tick)  # Tempo nach Jump aktualisieren
                    logger.info(f"[playback_loop] Jump to tick {self.current_tick} ausgeführt. New tempo: {current_tempo}")
                    start_time = current_time
                    last_tick_time = current_time
                
                # (2) Events verarbeiten
                if self.current_tick in self.events_by_tick:
                    events = self.events_by_tick[self.current_tick]
                    asyncio.create_task(self._process_tick_events(events, command_manager))
                
                # (3) Check auf Tempo-Änderung
                new_tempo = self.get_current_tempo(self.current_tick)
                if new_tempo != current_tempo:
                    # Bei Tempo-Änderung: Zeit-Basis neu setzen
                    start_time = current_time
                    elapsed_ticks = 0
                    current_tempo = new_tempo
                
                # (4) Berechne Zeit bis zum nächsten Tick
                sec_per_tick = 60.0 / (current_tempo * PPQ)
                target_time = start_time + (elapsed_ticks * sec_per_tick)
                
                # Warte bis zum nächsten Tick-Zeitpunkt
                sleep_time = target_time - current_time
                if sleep_time > 0:
                    await asyncio.sleep(sleep_time)
                
                # Debug output
                if self.current_tick % PPQ == 0:
                    now = asyncio.get_event_loop().time()
                    elapsed_since_last = now - last_tick_time
                    if elapsed_since_last > 0:
                        actual_bpm = 60.0 / elapsed_since_last
                        # logger.debug(f"[playback_loop] Beat {self.current_tick//PPQ}:")
                        # logger.debug(f"  - Current tempo: {current_tempo} BPM")
                        # logger.debug(f"  - Actual BPM: {actual_bpm:.1f}")
                    last_tick_time = now
                
                self.current_tick += 1
                elapsed_ticks += 1
            
            # Ende der Schleife
            self.is_playing = False
            logger.info("[playback_loop] Song zu Ende oder abgebrochen.")
            await self._on_playback_end()
        
        except asyncio.CancelledError:
            logger.info("[playback_loop] Cancelled.")
        except Exception as e:
            logger.error(f"[playback_loop] Fehler: {e}")
            self.is_playing = False
            await self._on_playback_end()

    async def _broadcast_position_loop(self):
        """
        Sendet periodisch (z.B. jede Sekunde) den aktuellen Tick an alle WebSockets.
        """
        last_broadcast_tick = -1
        try:
            while self.is_playing:
               if self.current_tick != last_broadcast_tick:
                  last_broadcast_tick = self.current_tick
                  msg = {
                        "type": "tick",
                        "tick": self.current_tick,
                        "timestamp": datetime.now().isoformat()
                  }
                  await websocket_manager.broadcast_message(msg)
                  # logger.debug(f"[broadcast_position_loop] -> {self.current_tick}")
                
               await asyncio.sleep(0.5)  # jede Sekunde
        except asyncio.CancelledError:
            pass
        except Exception as e:
            logger.error(f"[broadcast_position_loop] Fehler: {e}")

    async def _process_tick_events(self, events: List[MidiEvent], command_manager):
        """
        Verarbeitet die MIDI-Events, die an diesem Tick anliegen. 
        Hier nicht blockierend programmieren, sondern z.B. nur 
        Kommandos versenden und das meiste asynchron erledigen.
        """

        note_on_events = [e for e in events if e.type == 144 and e.velocity > 0]
        note_off_events = [e for e in events if e.type == 128 or (e.type == 144 and e.velocity == 0)]

        # Optional: Debug-Ausgaben
        logger.info(f"[Tick {self.current_tick}] {len(note_on_events)} ON, {len(note_off_events)} OFF")
        
        # 1) Note-Off-Events behandeln
        #    Beispiel: Register (1–15) ausschalten, etc.
        await self._handle_note_off_events(note_off_events)

        # 2) Note-On-Events behandeln
        await self._handle_note_on_events(note_on_events)

    async def stop(self):
        """
        Wiedergabe stoppen. Schließt beide Tasks (Playback und Broadcast).
        """
        async with self._player_lock:
            if not self.is_playing:
                return
            
            logger.info("[stop] Stoppe Wiedergabe.")
            self.is_playing = False
            
            # Tasks abbrechen
            if self._playback_task:
                self._playback_task.cancel()
            if self._broadcast_task:
                self._broadcast_task.cancel()

            # Reset-Befehl, Falls nötig:
            cmd = bytes([0x64, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0])
            await self.nano_manager.broadcast_command(cmd)
            await self._on_playback_end()

    async def _on_playback_end(self):
        """
        Cleanup, wenn das Abspielen fertig ist (oder abgebrochen).
        """
        # An WebSocket signalisieren
        if self.current_song:
            msg = {
                "type": "playback_ended",
                "song_name": self.current_song["songMetadata"]["name"],
                "status": "completed"
            }
            await websocket_manager.broadcast_message(msg)
            # Reset-Befehl, Falls nötig:
            cmd = bytes([0x64, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0])
            await self.nano_manager.broadcast_command(cmd)
            logger.info(f"[on_playback_end] -> {msg}")

    async def _interruptible_sleep(self, duration: float) -> None:
        """
        Sleep, der abgebrochen wird, sobald `self.is_playing=False` 
        oder ein Jump ansteht.
        """
        if not self.is_playing or self._jump_requested:
            return
        
        await asyncio.sleep(duration)

    async def jump_to_next_part(self) -> bool:
        """
        Springt zum Start des nächsten Parts oder (innerhalb von 30% des aktuellen Parts)
        zurück zum Start desselben Parts.
        """
        async with self._player_lock:
            if not self.is_playing or not self.parts_timeline:
                return False
            
            # Falls wir noch vor dem 1. Part sind:
            if self.current_tick < self.parts_timeline[0][0]:
                self._set_jump_target(self.parts_timeline[0][0])
                return True
            
            current_part = None
            next_part_start = None
            
            for (start, end) in self.parts_timeline:
                if start <= self.current_tick <= end:
                    current_part = (start, end)
                elif start > self.current_tick:
                    next_part_start = start
                    break
            
            if current_part:
                part_duration_ticks = (current_part[1] - current_part[0])
                ticks_into_part = (self.current_tick - current_part[0])
                
                if part_duration_ticks > 0:
                    fraction = ticks_into_part / part_duration_ticks
                    # Unter 30% => zurück an den Start
                    if fraction <= 0.3:
                        self._set_jump_target(current_part[0])
                        return True
            
            if next_part_start is not None:
                self._set_jump_target(next_part_start)
                return True
            
            logger.info("[jump_to_next_part] Kein nächster Part vorhanden (am letzten Part).")
            return False

    async def jump_to_tick(self, tick: int):
        """
        Extern aufrufbare Methode, um zu einem beliebigen Tick zu springen.
        """
        async with self._player_lock:
            if not self.is_playing:
                logger.info("[jump_to_tick] Song läuft nicht, ignoriere.")
                return
            self._set_jump_target(tick)

    def _set_jump_target(self, tick: int):
        """
        Interne Hilfsmethode, um einen Tick-Sprung zu setzen.
        Wird vom Loop in `_playback_loop` ausgewertet.
        """
        logger.info(f"[set_jump_target] Sprung angefordert zu Tick {tick}")
        self._jump_target = tick
        self._jump_requested = True
        self._is_holding = False

    async def _handle_note_off_events(self, note_off_events: List[MidiEvent]):
        # Beispiel: Register 1-15 "ausschalten"
        for event in note_off_events:
            # Optional: channel_0_active abfragen, wenn du Channel 0 als "Master" nutzen willst
            if event.channel == 0:
                self.channel_0_active = False

            if not self.channel_0_active: # wenn Channel 0 nicht aktiv ist, dann ignoriere die Note Off Events
                return True

            if 1 <= event.note <= 15:
                logger.info(f"[Tick {self.current_tick}] Note OFF | Register {event.note}")
                # Hier z.B. dein "Effect 100 (off)" Befehl an NanoManager
                command = [
                    0x64,  # 100 = "off"
                    0, 0,  # Dauer
                    0xFF,  # Intensität
                    0, 0, 0,  # RGB
                    0,  # Rainbow
                    0, 0,  # Speed
                    0,    # Length
                ]
                await self.nano_manager.broadcast_command(command, target_register=event.note)
                await asyncio.create_task(websocket_manager.broadcast_message({
                    "type": "midi_event",
                    "tick": self.current_tick,
                    "event": {
                        "type": "note_off",
                        "note": event.note,
                        "velocity": event.velocity,
                        "channel": event.channel,
                        "track": event.track
                    },
                    "settings": {
                        "register": [event.note]
                    }
                }))

    async def _handle_note_on_events(self, note_on_events: List[MidiEvent]):
        if not note_on_events:
            return
        
        # --- Effekt-Einstellungen sammeln ---
        # Erzeugt ein neues Settings-Objekt (per Default register=[1], rgb=[0,0,0])
        settings = EffectSettings()

        # Liste der Register, auf die Effekte angewendet werden
        registers = []

        # 1) Alle Noten durchgehen, Settings "zusammenbauen"
        for event in note_on_events:
            nt = event.note
            vel = event.velocity

            # 1–15: Register ansteuern
            if 1 <= nt <= 15:
                registers.append(nt)
            
            # 18–21: Farben
            elif nt == 18:  # Rot
                settings.rgb[0] = min(vel * 2, 255)
            elif nt == 19:  # Grün
                settings.rgb[1] = min(vel * 2, 255)
            elif nt == 20:  # Blau
                settings.rgb[2] = min(vel * 2, 255)
            elif nt == 21:  # Rainbow
                settings.rainbow = 1 if vel > 0 else 0
            
            # 25–28: Speed, Length, Intensity, ...
            elif nt == 26:
                settings.speed = vel
            elif nt == 27:
                settings.length = vel
            elif nt == 28:
                # Mach z.B. Intensity 0..255
                settings.intensity = min(int(vel * 2.55), 255)

            # Optional: Channel-0-Logik
            if event.channel == 0 and event.type == 144:
                self.channel_0_active = True
        
        # Falls wir Register gesammelt haben, übergeben wir sie in settings
        if registers:
            settings.register = registers

        # 2) Nun die Effekte auslösen
        #    z.B. bei Noten 30–56 oder 100–107 (wie im Original)
        for event in note_on_events:
            if (30 <= event.note <= 56) or (100 <= event.note <= 107):
                # Debug
                logger.info(f"[Tick {self.current_tick}] -> Effektnote {event.note}, Vel={event.velocity} | Settings={settings}")

                # Effekt anstoßen (asynchron) 
                await asyncio.create_task(effect_processor.process_effect(event.note, settings))
                # Optional: Warte auf das Ergebnis 
                # await effect_processor.process_effect(event.note, settings)

                # Optional: Broadcast an WebSocket
                await asyncio.create_task(websocket_manager.broadcast_message({
                    "type": "midi_event",
                    "settings": {
                        "effect": event.note,
                        "register": settings.register,
                        "rgb": settings.rgb,
                        "speed": settings.speed,
                        "length": settings.length,
                        "intensity": settings.intensity,
                        "rainbow": settings.rainbow
                    }
                }))

    async def hold(self):
        """Unterbricht das Abspielen temporär"""
        self._is_holding = True
        logger.info("[hold] Playback temporarily paused")
