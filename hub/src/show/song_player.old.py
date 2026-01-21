from fastapi import WebSocket
from typing import List, Optional, Dict
import asyncio
from dataclasses import dataclass
from ..websocket.websocket_manager import websocket_manager
from ..nano_network.nano_manager import NanoManager
from ..effects.coordinator_instance import coordinator
from ..effects.effect_processor import effect_processor, EffectSettings
from ..effects.effect_config import ColorEffectConfig
from .tsn_parser import TSNParser
from datetime import datetime

@dataclass
class MidiEvent:
   tick: int
   type: int  # 144 = Note On, 128 = Note Off
   note: int
   velocity: int
   channel: int
   track: int

class SongPlayer:
   def __init__(self):
      self.current_song = None
      self.parser = None
      self.timeline: List[MidiEvent] = []
      self.is_playing = False
      self.current_part_index = 0
      self.parts_timeline = []  # Liste von (start_tick, end_tick) Tupeln
      self.current_tick = 0  # Neue Variable zum Tracken der aktuellen Position
      self.events_by_tick = {}  # Neue Variable für gruppierte Events
      self.jump_requested = False  # Flag für Sprungwunsch
      self.jump_target = 0  # Ziel-Tick für Sprung
      self.jtt = None
      # Get the existing singleton instance
      self.nano_manager = NanoManager._instance or NanoManager()
      self.effect_coordinator = coordinator
      self.channel_0_active = False
      
   def load_song(self, tsn_content: str) -> bool:
      """Load and parse a TSN song file"""
      try:
         # Reset everything
         self.timeline = []
         self.current_song = None
         self.is_playing = False
         self.parser = TSNParser()
         
         # Parse new data
         parsed_data = self.parser.parse(tsn_content)
         self.current_song = parsed_data
         
         # Process all tracks and their parts
         for track in parsed_data['tracks']:
             track_num = track['trackNumber']
             channel = track['midiChannel']
             
             for part in track.get('parts', []):
                 part_start = part['timing']['start']
                 
                 # Add each note's events to timeline
                 for note in part.get('notes', []):
                     if note:  # Skip any None values
                         # Add the part's start tick to the note's relative tick
                         absolute_tick = part_start + note['tick']
                         
                         # Create MidiEvent for this note
                         event = MidiEvent(
                             tick=absolute_tick,
                             type=note['type'],
                             note=note['note'],
                             velocity=note['velocity'],
                             channel=channel,
                             track=track_num
                         )
                         self.timeline.append(event)
         
         # Sort timeline by absolute tick
         self.timeline.sort(key=lambda x: x.tick)
         
         # Group events by tick
         self._group_events()
         
         # Extrahiere Part-Grenzen
         self.parts_timeline = []
         current_tick = 0
         
         for track in parsed_data['tracks']:
             for part in track.get('parts', []):
                 start_tick = part['timing']['start']
                 if start_tick > current_tick:
                     self.parts_timeline.append((start_tick, None))
                     current_tick = start_tick
         
         # Sortiere und vervollständige die Timeline
         self.parts_timeline.sort(key=lambda x: x[0])
         for i in range(len(self.parts_timeline)-1):
             self.parts_timeline[i] = (
                 self.parts_timeline[i][0],
                 self.parts_timeline[i+1][0]
             )
         
         # Add end tick for last part if exists
         if self.parts_timeline:
             last_start = self.parts_timeline[-1][0]
             max_tick = max(event.tick for event in self.timeline) if self.timeline else last_start
             self.parts_timeline[-1] = (last_start, max_tick)
         
         return True
         
      except Exception as e:
         print(f"Error loading song: {str(e)}")
         return False

   def get_current_tempo(self, tick: int) -> int:
      """Get the effective tempo at the given tick position"""
      current_tempo = self.current_song['musicProperties']['masterTempo']
      
      tempo_changes = sorted(self.current_song.get('tempoChanges', []), key=lambda x: x['tick'])
      
      for change in tempo_changes:
         if change['tick'] <= tick and change['tempo'] > 0:
            print(f"Applying tempo change at tick {change['tick']}: {change['tempo']}")
            current_tempo = change['tempo']
      
      return current_tempo

   async def _build_state_for_position(self, target_tick: int, current_settings: dict):
      """Build the correct state for a given position"""
      print(f"Building state for position {target_tick}")
      
      # Reset all nanos first
      command = bytes([0x64, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0])
      await self.nano_manager.broadcast_command(command)
      await asyncio.sleep(0.1)  # Small delay to ensure reset
      
      # Find all setup events (register, color, etc.) up to this position
      setup_events = []
      for tick in sorted(self.events_by_tick.keys()):
         if tick > target_tick:
            break
         events = self.events_by_tick[tick]
         for event in events:
            # Collect all setup events (notes 1-15 for register, 18-21 for color, 25-28 for settings)
            if event.type == 144 and event.velocity > 0:  # Note On events
               if (0 <= event.note <= 15) or (18 <= event.note <= 21) or (25 <= event.note <= 28):
                  setup_events.append(event)
      
      # Reset current settings
      current_settings.update({
         'register': 0,
         'rgb': [0, 0, 0],
         'rainbow': 0,
         'speed': 0,
         'length': 0,
         'intensity': 255
      })
      
      # Apply all setup events to rebuild the state
      if setup_events:
         await self._process_events(setup_events, current_settings, None)
      
      print(f"State rebuilt for tick {target_tick}")
      return current_settings

   async def handle_websocket_message(self, message: dict, command_manager=None):
      """Handle incoming websocket messages"""
      if message['type'] == 'jump_to_tick':
         print(f"Received jump request to tick: {message['tick']}")
         self.jtt = message['tick']
         self.jump_requested = True

   async def play(self, command_manager, jump_to_tick=None) -> bool:
      if not self.current_song:
         return False
      
      try:
         current_settings = {
            'register': 0,
            'rgb': [0, 0, 0],
            'rainbow': 0,
            'speed': 0,
            'length': 0,
            'intensity': 255
         }
         
         print("\n=== Starting Song Playback ===")
         
         if not hasattr(self, 'events_by_tick'):
            self._group_events()
         
         sorted_ticks = sorted(self.events_by_tick.keys())
         if not sorted_ticks:
            print("No events found to play!")
            return False
         
         self.is_playing = True
         self.current_tick = jump_to_tick if jump_to_tick else self.parts_timeline[0][0]

         broadcast_task = asyncio.create_task(self._broadcast_current_tick())
         
         await self._build_state_for_position(self.current_tick, current_settings)
         
         i = 0
         while i < len(sorted_ticks) and sorted_ticks[i] < self.current_tick:
            i += 1
         
         last_jump_time = None
         while i < len(sorted_ticks) and self.is_playing:
            current_tick = sorted_ticks[i]

            # Überprüfe Sprungbefehle VOR der Wartezeit
            if (self.jump_requested or self.jtt is not None) and (
               last_jump_time is None or 
               (asyncio.get_event_loop().time() - last_jump_time) > 0.1
            ):
               target_tick = self.jtt if self.jtt is not None else self.jump_target
               print(f"\n=== Jumping to tick {target_tick} ===")
               
               await self._build_state_for_position(target_tick, current_settings)
               
               i = 0
               while i < len(sorted_ticks) and sorted_ticks[i] < target_tick:
                  i += 1
               self.current_tick = target_tick
               
               if target_tick in self.events_by_tick:
                  # Starte die Effektverarbeitung als separaten Task
                  asyncio.create_task(self._process_events(
                     self.events_by_tick[target_tick], 
                     current_settings, 
                     command_manager
                  ))
               
               self.jump_requested = False
               self.jtt = None
               last_jump_time = asyncio.get_event_loop().time()
               
               print(f"Playback continuing from tick {target_tick} (index {i})")
               continue
            
            if current_tick > self.current_tick:
               tick_diff = current_tick - self.current_tick
               delay = self.get_current_tempo_duration(self.current_tick) * tick_diff
               
               # Verwende _interruptible_sleep statt asyncio.sleep
               was_interrupted = await self._interruptible_sleep(delay)
               if was_interrupted:
                  continue
               
               self.current_tick = current_tick
            
            if current_tick in self.events_by_tick:
               # Starte die Effektverarbeitung als separaten Task
               asyncio.create_task(self._process_events(
                  self.events_by_tick[current_tick], 
                  current_settings, 
                  command_manager
               ))
            
            i += 1

         # Cleanup at end of playback
         self.is_playing = False
         broadcast_task.cancel()  # Cancel the broadcast task
         await self.stop()
         return True
         
      except Exception as e:
         print(f"Playback error: {str(e)}")
         self.is_playing = False
         await self.stop()
         return False

   async def _broadcast_current_tick(self):
      """Broadcasts the current tick every second"""
      last_tick = None
      while self.is_playing:
         try:
            current_tick = self.current_tick
            # Only broadcast if the tick has changed and is valid
            if current_tick != last_tick and current_tick >= 0:
                await websocket_manager.broadcast_message({
                   "type": "tick",
                   "tick": current_tick,
                   "timestamp": datetime.now().isoformat()
                })
                last_tick = current_tick
            await asyncio.sleep(0.25)  # Check every 250ms - weniger häufig
         except Exception as e:
            print(f"Error broadcasting tick: {str(e)}")
            await asyncio.sleep(1)

   async def _process_events(self, events, current_settings, command_manager):
      """Verarbeitet die Events an einem bestimmten Tick"""
      tick_duration = self.get_current_tempo_duration(self.current_tick)
      
      # Separate note on and off events
      note_on_events = [e for e in events if e.type == 144 and e.velocity > 0]
      note_off_events = [e for e in events if e.type == 128 or (e.type == 144 and e.velocity == 0)]

      # TODO: Hier das einbauen
      # # Check Channel 0 events first
      # for event in events:
      #    if event.channel == 0:
      #       print(f"------Event: {event}")
      #       if event.type == 144:
      #          print(f"► Channel 0 activated")
      #          self.channel_0_active = True
      #       elif event.type == 128:  # Note Off
      #          print(f"► Channel 0 deactivated")
      #          self.channel_0_active = False
      # self.channel_0_active = True


      # Process register off events only if Channel 0 is active
      for event in note_off_events:
         if 1 <= event.note <= 15 and self.channel_0_active:  # Register channels
            print(f"► Note Off | Ch: {event.channel:2d} | Register: {event.note:2d}")
            command = bytes([
               0x64,  # Effect 100 (off)
               0, 0,  # Duration
               0xFF,  # Intensity
               0, 0, 0,  # RGB
               0,  # Rainbow
               0, 0,  # Speed
               0  # Length
            ])
            await self.nano_manager.broadcast_command(command, target_register=event.note)

      if note_on_events:
         print(f"\n=== Tick {self.current_tick} ===")

         settings = EffectSettings(
             register=0,  # Dies wird jetzt eine Liste von Registern
             rgb=[0, 0, 0],
             rainbow=0,
             speed=0,
             length=0,
             intensity=255
         )

         # Neue Liste für Register
         registers = []

         # Erst alle Einstellungen sammeln
         for event in note_on_events:
            nt = event.note
            vel = event.velocity
            
            # Register channels (1-15)
            if 1 <= nt <= 15:
               registers.append(nt)  # Sammle alle Register
            # Color channels (18-21)
            elif nt == 18:  # Red
               settings.rgb[0] = int(vel * 2)
            elif nt == 19:  # Green
               settings.rgb[1] = int(vel * 2)
            elif nt == 20:  # Blue
               settings.rgb[2] = int(vel * 2)
            elif nt == 21:  # Rainbow
               settings.rainbow = 1 if vel > 0 else 0
            # Settings channels (25-28)
            elif nt == 26:  # Speed
               settings.speed = vel
            elif nt == 27:  # Length
               settings.length = vel
            elif nt == 28:  # Intensity
               settings.intensity = min(int(vel * 2.55), 255)

         # Setze die gesammelten Register
         settings.register = registers

         # Dann die Effekte ausführen
         for event in note_on_events:
            if 30 <= event.note <= 56 or 100 <= event.note <= 107:
               print(f"► Note On  | Ch: {event.channel:2d} | Note: {event.note:3d} | Vel: {event.velocity:3d}")
               print(f"► Applying effect to registers: {registers}")
               start_time = datetime.now()
               await effect_processor.process_effect(event.note, settings)
               end_time = datetime.now()
               duration = (end_time - start_time).total_seconds()
               print(f"Effect processing time: {duration:.6f} seconds")

         print(f"Debug: Settings after processing events: {settings}")

   async def stop(self):
      """Stop the currently playing song"""
      self.is_playing = False
      
      # Send stop command (code 1000) to all nanos
      command = bytes([0x64, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0])
      await self.nano_manager.broadcast_command(command)
      
      await websocket_manager.broadcast_message({
            "type": "playback_ended",
            "song_name": self.current_song["songMetadata"]["name"],
            "status": "completed"
      })

   def _group_events(self):
      """Gruppiere Events nach Ticks"""
      self.events_by_tick = {}
      if not hasattr(self, 'timeline') or not self.timeline:
         print("Warning: No timeline available for grouping events")
         return
        
      print(f"Debug: Grouping {len(self.timeline)} events")
      for event in self.timeline:
         if event.tick not in self.events_by_tick:
            self.events_by_tick[event.tick] = []
         self.events_by_tick[event.tick].append(event)
      
      print(f"Debug: Grouped events into {len(self.events_by_tick)} unique ticks")

   def jump_to_next_part(self) -> bool:
      """
      Jump to next part or back to start of current part.
      - If called within first 30% of part duration: Jump back to start of current part
      - Otherwise: Jump to next part
      """
      if not self.is_playing or not self.timeline:
         return False
      
      # Find current part
      current_part_start = None
      current_part_end = None
      next_part_start = None
      
      # Ensure we're checking the first part correctly
      if self.parts_timeline and self.current_tick < self.parts_timeline[0][0]:
         # We're before the first part, so jump to it
         print(f"\n=== Springe zum ersten Part (Tick {self.parts_timeline[0][0]}) ===")
         self.jump_requested = True
         self.jump_target = self.parts_timeline[0][0]
         return True
      
      for start, end in self.parts_timeline:
         if start <= self.current_tick <= end:
            current_part_start = start
            current_part_end = end
         elif start > self.current_tick:
            next_part_start = start
            break
      
      if current_part_start is not None:
         # Berechne die Dauer pro Tick einmal
         tick_duration = self.get_current_tempo_duration(current_part_start)
         
         # Berechne Gesamtdauer und verstrichene Zeit
         total_part_duration = (current_part_end - current_part_start) * tick_duration
         time_since_start = (self.current_tick - current_part_start) * tick_duration
         
         # Debug-Ausgaben
         print(f"\nDebug Jump-Calculation:")
         print(f"Current tick: {self.current_tick}")
         print(f"Part start: {current_part_start}")
         print(f"Part end: {current_part_end}")
         print(f"Total part duration: {total_part_duration:.2f}s")
         print(f"Time since start: {time_since_start:.2f}s")
         print(f"Percentage of part: {(time_since_start/total_part_duration)*100:.1f}%")
         print(f"Will jump back: {time_since_start <= (total_part_duration * 0.3)}")
         
         if time_since_start <= (total_part_duration * 0.3):
            print(f"\n=== Springe zurück zum Start des aktuellen Parts (Tick {current_part_start}) ===")
            self.jump_requested = True
            self.jump_target = current_part_start
            return True
      
      # Sonst zum nächsten Part springen
      if next_part_start is None:
         print("Bereits im letzten Part")
         return False
      
      print(f"\n=== Springe zum nächsten Part (Tick {next_part_start}) ===")
      self.jump_requested = True
      self.jump_target = next_part_start
      return True

   async def _interruptible_sleep(self, duration: float) -> bool:
      """
      Sleep that can be interrupted by jump requests
      Returns True if interrupted, False if completed normally
      """
      start_time = asyncio.get_event_loop().time()
      check_interval = 0.01  # 10ms check interval
      
      while (asyncio.get_event_loop().time() - start_time) < duration:
         if not self.is_playing:
            return True
         if self.jump_requested or self.jtt is not None:
            return True
         await asyncio.sleep(check_interval)
      
      return False
      
   def get_current_tempo_duration(self, tick: int) -> float:
      """Get the duration of a single tick at the current position"""
      current_tempo = self.get_current_tempo(tick)
      PPQ = 96  # TSN uses 96 ticks per quarter note (fixed)
      
      # Ein Beat (Viertelnote) bei z.B. 144 BPM dauert 60/144 Sekunden
      seconds_per_beat = 60.0 / current_tempo
      
      # Berechne die Zeit pro Tick
      seconds_per_tick = seconds_per_beat / PPQ
      
      return seconds_per_tick

