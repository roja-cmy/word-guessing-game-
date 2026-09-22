assets/sounds/  —  sound-effect placeholders
=========================================

WORD HUNT looks for these (optional) files. Drop them in this folder with exactly
these names and they will be used automatically:

  click.mp3     – short UI click / button press
  correct.mp3   – a correct letter was revealed
  wrong.mp3     – a wrong guess / life lost
  hint.mp3      – the HINT button was used
  win.mp3       – victory fanfare
  lose.mp3      – game-over sound

If a file is missing, the game automatically falls back to a short sound that is
synthesised with the browser's WebAudio API, so the project never depends on
external (or copyrighted) audio. Sound can be muted with the "Sound" button on
the home screen.

Recommended: 44.1 kHz mono MP3 or OGG, 0.2–1.5 seconds, ≤ 60 KB each.
Use only audio you made yourself or that is licensed for reuse (e.g. CC0).
