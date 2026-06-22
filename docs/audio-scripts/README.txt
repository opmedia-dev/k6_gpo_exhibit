K6 GPO Exhibit — Audio Scripts
==============================

These scripts are intended for recording as MP3 audio files for the
K6 telephone exhibit. Record them using a clear British voice, or use
a text-to-speech service. All files should be mono, 8kHz or higher
sample rate, MP3 format.

The scripts are organised to match the SD card directory structure
expected by the firmware.


HISTORY FILES — /history/
-------------------------
Played in random order when a visitor answers an auto-ring call.

  k6_history.mp3      The story of the K6 telephone box (~45s)
  gpo_exchange.mp3    How calls were connected by operators (~50s)
  rotary_dialling.mp3 How the rotary dial works (~40s)
  speaking_clock.mp3  The story of TIM / Speaking Clock (~45s)
  999_history.mp3     The world's first emergency number (~45s)
  button_ab.mp3       How Button A and Button B worked (~45s)
  model_railways.mp3  Telephones and the railway (~50s)


NUMBER FILES — /numbers/
------------------------
Played when a visitor dials the corresponding number.

  Dial    File                  Description
  ----    ----                  -----------
  100     operator.mp3          GPO operator — "Number please?"
  0       operator.mp3          (same file, mapped to both numbers)
  123     speaking_clock.mp3    TIM speaking clock demonstration
  999     emergency.mp3         Emergency services demonstration
  192     directory.mp3         Directory enquiries
  150     faults.mp3            GPO faults reporting line
  246     station_master.mp3    Station master's office
  347     departures.mp3        Train departure announcements
  501     signal_box.mp3        Signal box / signalman
  742     parcels.mp3           Railway parcels office
  800     timetable.mp3         Timetable enquiries
  007     bond.mp3              Easter egg — spy theme
  666     spooky.mp3            Easter egg — ghost train


SYSTEM FILES — /system/
-----------------------
Used by the firmware for specific events.

  not_recognised.mp3  Played when a dialled number is not in the directory
  welcome.mp3         Optional welcome message for auto-ring answers
  insert_coins.mp3    Prompt to insert coins (coinbox mode only)
  press_a.mp3         Prompt to press Button A (coinbox mode only)


RECORDING TIPS
--------------
- Use a quiet room with minimal echo
- Speak clearly at a moderate pace
- Leave a 0.5-second silence at the start and end of each file
- Target volume: -16 dBFS peak (loud enough to hear through
  the telephone earpiece without distortion)
- For sound effects (static, reverb, bells), add them in post-
  production rather than trying to record them live
- The telephone earpiece has limited frequency response — avoid
  relying on very high or very low frequencies for important content
