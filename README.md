# MIDI Keyboard Display & Sampler

A JUCE-based VST3/AU plugin that displays MIDI input on a visual keyboard and plays back samples mapped to notes, velocities, and round-robin positions.

## Features

- Visual 3-octave keyboard display (C3-B5)
- Per-note grid showing velocity tiers and round-robin positions
- Sample playback with velocity layers and round-robin cycling
- Sustain pedal support

## Sample Folder Setup

Point the plugin to a folder containing your audio samples. Samples must follow a specific naming convention to be mapped correctly.

### File Naming Convention

```
NoteName_Velocity_RoundRobin[_OptionalSuffix].wav
```

**Components:**
- **NoteName**: Note name with octave (e.g., `C4`, `G#6`, `Db3`, `F#5`)
  - Sharps use `#` (e.g., `C#4`, `F#3`)
  - Flats use `b` (e.g., `Db4`, `Bb3`)
  - Case insensitive
- **Velocity**: 3-digit velocity value, zero-padded (e.g., `001`, `033`, `127`)
- **RoundRobin**: 2-digit round-robin position, zero-padded (e.g., `01`, `02`, `03`)
- **OptionalSuffix**: Any additional text after the RR value is ignored (for organization)

**Examples:**
```
C4_001_02.wav       → C4, velocity 1, round-robin 2
g#6_033_01.wav      → G#6, velocity 33, round-robin 1
Db3_127_03.wav      → Db3, velocity 127, round-robin 3
F#5_088_01_soft.wav → F#5, velocity 88, round-robin 1 (suffix ignored)
```

### Velocity Range Mapping

Samples don't need to cover all 127 velocity values. The plugin automatically creates velocity ranges based on available samples.

**How it works:**
- Each velocity value in your samples covers a range from itself down to the next lower available velocity + 1
- The lowest velocity sample covers from velocity 1 up to its value

**Example with velocities 127, 126, 88, 40:**
| Sample Velocity | Triggers on MIDI Velocities |
|-----------------|----------------------------|
| 127             | 127 only                   |
| 126             | 89 - 126                   |
| 88              | 41 - 88                    |
| 40              | 1 - 40                     |

**Example with velocities 127, 64, 1:**
| Sample Velocity | Triggers on MIDI Velocities |
|-----------------|----------------------------|
| 127             | 65 - 127                   |
| 64              | 2 - 64                     |
| 1               | 1 only                     |

### Missing Notes

If a note has no samples available:
- The plugin uses the **first available sample higher in pitch**
- **Pitch-shifting is applied** - the sample is transposed down to sound at the correct pitch
- Uses high-quality linear interpolation for smooth pitch shifting

**Example:**
If you have samples for C4, E4, and G4, but play D4:
- D4 will trigger the E4 sample (next higher available note)
- The sample is pitch-shifted down 2 semitones to sound like D4

### Round-Robin

Round-robin cycles through available samples (1 → 2 → 3 → 1) for natural variation when repeatedly playing the same note at similar velocities.

## Visual Display

The plugin shows a grid above the keyboard:
- **Columns**: One per note (36 notes, C3-B5)
- **Rows**: 3 velocity tiers (High: 85-127, Mid: 43-84, Low: 1-42)
- **Cells**: 3 round-robin boxes per tier cell

When a note is played:
- The corresponding key lights up blue
- The velocity tier row activates
- The round-robin box shows which position triggered

With sustain pedal held, all triggered states accumulate and persist until pedal release.

## Building

Requires JUCE framework installed at `~/JUCE`.

```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

Output plugins are in `build/MidiKeyboardDisplay_artefacts/Release/`:
- `VST3/MIDI Keyboard Display.vst3`
- `AU/MIDI Keyboard Display.component`
- `Standalone/MIDI Keyboard Display.app`

## Author

ALDENHammersmith

## License

MIT
