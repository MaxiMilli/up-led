# Einen Song abspielen


1. Starte den Entwicklungsserver:
```bash
python -m src.main
```

2. Song importieren, falls nicht geschehen (Das ist ein einfacher Beispielsong -> Muss vermutlich angepasst wereden):
```curl
curl --location 'http://localhost:8000/songs/import/show2' \
--header 'Content-Type: text/plain' \
--data '<CAS Song>
 <Data>
  SongName='\''D:\NFG\NFG_Songs\Uzepatscher 2023\show.tsn'\''
  DataVersion=7
  SubVersion=0
  DecVersion=0
  MasterTempo=144
  BeatStyle=BS_16tel
  SwingType=st_8tel
  NotenMin=BS_16tel
  NotenIdeal=BS_8tel
 </Data>

 <TaktArtList>
  <TaktArtField>
   beginC=0
   endC=767615
   FirstNum=1
   LastNum=1999
  </TaktArtField>
 </TaktArtList>

 <TuneList>
  <TonartObj>
   Tick=0
   BarNr=1
   Tonart=9
  </TonartObj>
 </TuneList>

 <MetaEvents>
  <MetaMsg>
   Tck=768
   type=6
   data='\''Intro 1'\''
  </MetaMsg>
  <MetaMsg>
   Tck=4224
   type=6
   data='\''Bass Solo'\''
  </MetaMsg>
  <MetaMsg>
   Tck=7296
   type=6
   data='\''Str.1'\''
  </MetaMsg>
 </MetaEvents>

 <TrackList>
  <TrackData>
   TrackNr=1
   TrackName='\''RECORD'\''
   MidiChannel=0
  <InstrDef>
   DeviceName='\''General Midi'\''
   FamilyName='\''Piano'\''
   InstrName='\''acoustic Grand Piano'\''
  </InstrDef>
   yFirst=119
   yNext=224
   ySec=176
   yText=120
   yChord=45
   GitiChordsOn=FALSE
   ShowInMainScore=TRUE
   FamilyColorNr=4210912
  </TrackData>
  <TrackData>
   TrackNr=7
   TrackName='\''Trp'\''
   MidiChannel=0
  <InstrDef>
   DeviceName='\''General Midi'\''
   FamilyName='\''Brass'\''
   InstrName='\''Trumpet'\''
  </InstrDef>
   yFirst=119
   yNext=224
   ySec=176
   yText=120
   yChord=45
   GitiChordsOn=FALSE
   ShowInMainScore=TRUE
   MapNr=-1
   FamilyColorNr=4210912
  <PartList>
   <PartData>
    PartName='\''Clip'\''
    BeatStyle=BS_16tel
    PartStart=384
    PartEnd=7295
    TaktNum=18
    StartTaktNr=1
    <NoteList>
     <Msg=720,144,74,105,0/>
     <Msg=720,144,69,105,0/>
     <Msg=757,128,74,0,0/>
     <Msg=757,128,69,0,0/>
    </NoteList>
   </PartData>
  </PartList>
  </TrackData>
  <TrackData>
   TrackNr=8
   TrackName='\''LED'\''
   MidiChannel=1
   HdrVolume=105
  <InstrDef>
   DeviceName='\''General Midi'\''
   FamilyName='\''Brass'\''
   InstrName='\''Trombone'\''
  </InstrDef>
   yFirst=119
   yNext=224
   ySec=176
   yText=120
   yChord=45
   GitiChordsOn=FALSE
   ShowInMainScore=TRUE
   MapNr=-1
   FamilyColorNr=8446016
  <PartList>
   <PartData>
    PartName='\''New'\''
    BeatStyle=BS_16tel
    PartStart=0
    PartEnd=383
    TaktNum=1
    StartTaktNr=0
   </PartData>
   <PartData>
    PartName='\''Clip'\''
    BeatStyle=BS_16tel
    PartStart=384
    PartEnd=1141
    TaktNum=10
    StartTaktNr=1
    <NoteList>
     <Msg=0,144,62,105,0/>
     <Msg=0,144,57,105,0/>
     <Msg=0,144,50,105,0/>
     <Msg=757,128,62,0,0/>
     <Msg=757,128,57,0,0/>
     <Msg=757,128,50,0,0/>
    </NoteList>
   </PartData>
   <PartData>
    PartName='\''New'\''
    BeatStyle=BS_16tel
    PartStart=1142
    PartEnd=2000
    TaktNum=4
    StartTaktNr=15
    <NoteList>
     <Msg=0,144,52,100,0/>
     <Msg=0,144,47,100,0/>
     <Msg=858,128,52,0,0/>
     <Msg=858,128,47,0,0/>
    </NoteList>
   </PartData>
  </PartList>
  </TrackData>
 </TrackList>
</CAS Song>'
```

3. Song vorbereiten:
```bash
curl --location --request POST 'http://localhost:8000/songs/show2/load'
```

Gibt folgende Logs:
```
Lade TSN-Datei: /Users/samuelrhyner/Library/CloudStorage/GoogleDrive-samuel.rhyner.sr@gmail.com/My Drive/Uzepatscher/LED/uzepatscher-led-gwaendli/hub/songs/show2_v1_20241128_143625.tsn
Debug - Created new part: {'name': '', 'beatStyle': '', 'timing': {'start': 0, 'end': 0, 'measureCount': 0, 'startMeasure': 0}, 'notes': []}
Setting PartStart to 384
Debug - Created new part: {'name': '', 'beatStyle': '', 'timing': {'start': 0, 'end': 0, 'measureCount': 0, 'startMeasure': 0}, 'notes': []}
Setting PartStart to 0
Debug - Created new part: {'name': '', 'beatStyle': '', 'timing': {'start': 0, 'end': 0, 'measureCount': 0, 'startMeasure': 0}, 'notes': []}
Setting PartStart to 384
Debug - Created new part: {'name': '', 'beatStyle': '', 'timing': {'start': 0, 'end': 0, 'measureCount': 0, 'startMeasure': 0}, 'notes': []}
Setting PartStart to 1142
INFO:     127.0.0.1:53318 - "POST /songs/show2/load HTTP/1.1" 200 OK
```

4. Song abspielen:
```bash
curl --location --request POST 'http://localhost:8000/songs/play'
```

Gibt folgende Logs:
```
INFO:     127.0.0.1:53318 - "POST /songs/play HTTP/1.1" 200 OK

=== Starte Song-Wiedergabe ===
Tempo: 144 BPM
Debug: 0.000868 seconds per tick
► Note On  | Tick:    384 | Note:  62 | Vel: 105 | Ch:  1
► Note On  | Tick:    384 | Note:  57 | Vel: 105 | Ch:  1
► Note On  | Tick:    384 | Note:  50 | Vel: 105 | Ch:  1
► Note On  | Tick:   1104 | Note:  74 | Vel: 105 | Ch:  0
► Note On  | Tick:   1104 | Note:  69 | Vel: 105 | Ch:  0
▼ Note Off | Tick:   1141 | Note:  74 | Vel:   0 | Ch:  0
▼ Note Off | Tick:   1141 | Note:  69 | Vel:   0 | Ch:  0
▼ Note Off | Tick:   1141 | Note:  62 | Vel:   0 | Ch:  1
▼ Note Off | Tick:   1141 | Note:  57 | Vel:   0 | Ch:  1
▼ Note Off | Tick:   1141 | Note:  50 | Vel:   0 | Ch:  1
► Note On  | Tick:   1142 | Note:  52 | Vel: 100 | Ch:  1
► Note On  | Tick:   1142 | Note:  47 | Vel: 100 | Ch:  1
▼ Note Off | Tick:   2000 | Note:  52 | Vel:   0 | Ch:  1
▼ Note Off | Tick:   2000 | Note:  47 | Vel:   0 | Ch:  1

=== Wiedergabe beendet ===

```

