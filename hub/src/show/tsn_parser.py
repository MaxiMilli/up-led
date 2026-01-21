from dataclasses import dataclass, field
from typing import List, Dict, Any, Optional
import json
import os

@dataclass
class TSNParser:
   def __init__(self):
      self.data = {
         "songMetadata": {
               "name": "",
               "path": "",
               "version": {
                  "dataVersion": 0,
                  "subVersion": 0,
                  "decVersion": 0
               }
         },
         "musicProperties": {
               "masterTempo": 0,
               "beatStyle": "",
               "swingType": "",
               "noteProperties": {
                  "minimum": "",
                  "ideal": ""
               }
         },
         "barStructure": {
               "measures": []
         },
         "keySignature": [],
         "markers": [],
         "tracks": [],
         "tempoChanges": []
      }
      self._current_track = None
      self._current_part = None
      self._in_track_data = False
      self._in_part_data = False
      self._in_note_list = False

   def _parse_song_path(self, value: str) -> None:
      """Parse song path and extract name and directory"""
      path = value.strip("'")
      self.data["songMetadata"]["name"] = os.path.basename(path)
      self.data["songMetadata"]["path"] = os.path.dirname(path)

   def _parse_midi_message(self, msg_str: str) -> Dict[str, int]:
      """Parse MIDI message string into dictionary"""
      try:
         # Remove <Msg= and />
         msg_data = msg_str[5:-2] if msg_str.endswith('/>') else msg_str[5:-1]
         parts = msg_data.split(',')
         if len(parts) != 5:
            print(f"Warning: Invalid MIDI message format: {msg_str}")
            return None
         
         tick, msg_type, note, velocity, channel = map(int, parts)
         return {
            "tick": tick,
            "type": msg_type,
            "note": note,
            "velocity": velocity,
            "channel": channel
         }
      except Exception as e:
         print(f"Error parsing MIDI message: {msg_str}")
         print(f"Error details: {str(e)}")
         return None

   def _init_new_track(self) -> Dict[str, Any]:
      """Initialize a new track structure"""
      return {
         "trackNumber": 0,
         "name": "",
         "midiChannel": 0,
         "instrument": {
               "device": "",
               "family": "",
               "name": ""
         },
         "display": {
               "yPositions": {
                  "first": 0,
                  "next": 0,
                  "secondary": 0,
                  "text": 0,
                  "chord": 0
               },
               "guitarChordsEnabled": False,
               "showInMainScore": True,
               "familyColorNumber": 0
         },
         "parts": []
      }

   def _init_new_part(self) -> Dict[str, Any]:
      """Initialize a new part structure"""
      return {
         "name": "",
         "beatStyle": "",
         "timing": {
               "start": 0,
               "end": 0,
               "measureCount": 0,
               "startMeasure": 0
         },
         "notes": []
      }

   def _parse_global_ctrl(self, line: str):
      """Parse global control messages including tempo changes"""
      if not line.startswith('<Msg='):
         return
        
      try:
         # Remove '<Msg=' and '/>' and split by comma
         parts = line[5:-2].split(',')
         if len(parts) != 5:  # Tempo messages have 5 parts
            return
            
         tick = int(parts[0])
         msg_type = int(parts[1])
         
         # Check if this is a tempo message (type 16)
         if msg_type == 16:
            new_tempo = int(parts[4])
            if 'tempoChanges' not in self.data:
               self.data['tempoChanges'] = []
            self.data['tempoChanges'].append({
               'tick': tick,
               'tempo': new_tempo
            })
            print(f"DEBUG: Tempo change at tick {tick}: {new_tempo} BPM")
      except Exception as e:
         print(f"Error parsing global control message: {str(e)}")

   def parse(self, content: str) -> Dict[str, Any]:
      """Parse TSN content into JSON structure"""
      lines = content.split('\n')
      is_led_track = False  # Flag to track if we're in an LED track
      in_global_ctrl = False  # Flag für GlobalCtrlColl Sektion
      
      for line in lines:
         line = line.strip()
         if not line:
            continue

         # Handle GlobalCtrlColl section
         if line == '<GlobalCtrlColl>':
            in_global_ctrl = True
            continue
         elif line == '</GlobalCtrlColl>':
            in_global_ctrl = False
            continue
         
         # Parse global control messages (including tempo changes)
         if in_global_ctrl:
            self._parse_global_ctrl(line)
            continue

         # Check for LED track
         if 'TrackName=' in line and "'LED'" in line:
            is_led_track = True
            # print("DEBUG: LED Track gefunden")
         elif line.startswith('</TrackData>'):
            if self._current_track and is_led_track:
               # print(f"DEBUG: Track hinzugefügt: {self._current_track['name']} mit {len(self._current_track['parts'])} Parts")
               self.data['tracks'].append(self._current_track)
            self._in_track_data = False
            self._current_track = None
            is_led_track = False
            # print("DEBUG: Ende des LED Tracks")
            continue

         # Handle section markers
         if line.startswith('<') and line.endswith('>'):
            if line.startswith('<Msg='):
               # This is a MIDI message, not a section marker
               if self._in_note_list and self._current_part and is_led_track:
                  msg = self._parse_midi_message(line)
                  if msg:  # Only append if message was parsed successfully
                     self._current_part['notes'].append(msg)
                     # print(f"DEBUG: Note hinzugefügt zu Part {self._current_part['name']}")
            else:
               section = line.strip('<>')
               if section == 'TrackData':
                  self._in_track_data = True
                  self._current_track = self._init_new_track()
                  # print("DEBUG: Neuer Track initialisiert")
               elif section == '/TrackData':
                  if self._current_track and is_led_track:
                     # print(f"DEBUG: Track abgeschlossen: {self._current_track['name']}")
                     self.data['tracks'].append(self._current_track)
                  self._in_track_data = False
                  self._current_track = None
                  is_led_track = False
               elif section == 'PartData' and is_led_track:
                  self._in_part_data = True
                  self._current_part = self._init_new_part()
                  # print("DEBUG: Neuer Part initialisiert")
               elif section == '/PartData' and is_led_track:
                  if self._current_part and self._current_track:
                     self._current_track['parts'].append(self._current_part)
                     # print(f"DEBUG: Part hinzugefügt zu Track {self._current_track['name']}")
                  self._in_part_data = False
                  self._current_part = None
               elif section == 'NoteList' and is_led_track:
                  self._in_note_list = True
               elif section == '/NoteList' and is_led_track:
                  self._in_note_list = False
            continue

         # Handle key-value pairs only for LED tracks
         if '=' in line and (not self._in_track_data or is_led_track):
            key, value = [x.strip() for x in line.split('=', 1)]
            value = value.strip("'")
            
            # Handle main song data (outside of tracks)
            if not self._in_track_data:
               if key == 'SongName':
                  self._parse_song_path(value)
               elif key == 'DataVersion':
                  self.data['songMetadata']['version']['dataVersion'] = int(value)
               elif key == 'SubVersion':
                  self.data['songMetadata']['version']['subVersion'] = int(value)
               elif key == 'DecVersion':
                  self.data['songMetadata']['version']['decVersion'] = int(value)
               elif key == 'MasterTempo':
                  self.data['musicProperties']['masterTempo'] = int(value)
               elif key == 'BeatStyle':
                  self.data['musicProperties']['beatStyle'] = value
               elif key == 'SwingType':
                  self.data['musicProperties']['swingType'] = value
               elif key == 'NotenMin':
                  self.data['musicProperties']['noteProperties']['minimum'] = value
               elif key == 'NotenIdeal':
                  self.data['musicProperties']['noteProperties']['ideal'] = value
            
            # Handle track data only for LED tracks
            elif self._in_track_data and self._current_track and is_led_track:
               if key == 'TrackNr':
                  self._current_track['trackNumber'] = int(value)
               elif key == 'TrackName':
                  self._current_track['name'] = value
               elif key == 'MidiChannel':
                  self._current_track['midiChannel'] = int(value)
               elif key == 'DeviceName':
                  self._current_track['instrument']['device'] = value
               elif key == 'FamilyName':
                  self._current_track['instrument']['family'] = value
               elif key == 'InstrName':
                  self._current_track['instrument']['name'] = value
               elif key == 'yFirst':
                  self._current_track['display']['yPositions']['first'] = int(value)
               elif key == 'yNext':
                  self._current_track['display']['yPositions']['next'] = int(value)
               elif key == 'ySec':
                  self._current_track['display']['yPositions']['secondary'] = int(value)
               elif key == 'yText':
                  self._current_track['display']['yPositions']['text'] = int(value)
               elif key == 'yChord':
                  self._current_track['display']['yPositions']['chord'] = int(value)
               elif key == 'GitiChordsOn':
                  self._current_track['display']['guitarChordsEnabled'] = value.upper() == 'TRUE'
               elif key == 'ShowInMainScore':
                  self._current_track['display']['showInMainScore'] = value.upper() == 'TRUE'
               elif key == 'FamilyColorNr':
                  self._current_track['display']['familyColorNumber'] = int(value)

               # Handle part data
               elif self._in_part_data and self._current_part:
                  if key == 'PartName':
                     self._current_part['name'] = value
                  elif key == 'BeatStyle':
                     self._current_part['beatStyle'] = value
                  elif key == 'PartStart':
                     # print(f"Setting PartStart to {value}")
                     self._current_part['timing']['start'] = int(value)
                  elif key == 'PartEnd':
                     # print(f"Setting PartEnd to {value}")
                     self._current_part['timing']['end'] = int(value)
                  elif key == 'TaktNum':
                     self._current_part['timing']['measureCount'] = int(value)
                  elif key == 'StartTaktNr':
                     self._current_part['timing']['startMeasure'] = int(value)

      # Debug output to verify note parsing
      # print("\n=== Debug: Note Counts ===")
      # for track in self.data['tracks']:
      #    print(f"Track: {track['name']}")
      #    for i, part in enumerate(track.get('parts', [])):
      #       print(f"  Part {i+1}: {len(part.get('notes', []))} notes")
      # print("========================\n")

      print("\n=== Parsing Summary ===")
      print(f"Total tracks: {len(self.data['tracks'])}")
      for track in self.data['tracks']:
         print(f"Track '{track['name']}' has {len(track['parts'])} parts")
         for i, part in enumerate(track['parts']):
            print(f"  Part {i+1}: {part['name']} with {len(part['notes'])} notes")
      print("=====================\n")

      return self.data

   def get_tick_duration(self, current_tick: int) -> float:
      """Calculate duration of one tick at the given position"""
      current_tempo = self.data['musicProperties']['masterTempo']
      
      # Find applicable tempo change
      for change in self.data['tempoChanges']:
         if change['tick'] <= current_tick:
            current_tempo = change['tempo']
         else:
            break
      
      # Calculate tick duration in seconds
      PPQ = 480  # TSN uses 480 ticks per quarter note
      seconds_per_minute = 60.0
      seconds_per_beat = seconds_per_minute / current_tempo
      seconds_per_tick = seconds_per_beat / PPQ
      
      return seconds_per_tick
      
def parse_tsn_file(content: str) -> Dict[str, Any]:
   """Parse TSN content and return JSON structure"""
   parser = TSNParser()
   return parser.parse(content)
