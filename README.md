# ReChord

VST3 instrument for Cubase by **Pouria Motabean**.
Write 4 chords, see how every note travels to the next chord, fix the connections, and drag the 4 bars out as a looping WAV —
all notes glide together (polyphonic glide), with a sustained FM lead sound.

- display: every note of every chord + its path to the next chord (orange = glide, grey = the note ends and a new one starts)
  - drag a note onto a note of the next (or previous) chord = connect them
  - click a line = glide / end
  - drag a note up / down = one octave
- chord strip: click a chord = pick root + kind (maj, min, 7, maj7, m7, sus2, sus4, dim)
- SMART = fresh smart voicing (smallest movement around the whole cycle)
- GLIDE: EARLY (starts before the bar line), LATE (lands after it), CURVE, SPREAD (top voice first)
- WITH CUBASE: plays along with the project (bar 1 = chord 1); PLAY: preview while Cubase is stopped
- DRAG WAV: the 4 bars at the project tempo, seamless loop (also saved in Documents\ReChord\Renders)

Built by GitHub Actions (Windows): download **ReChord-Setup-Windows** from the latest run.
