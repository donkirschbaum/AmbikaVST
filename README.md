# Ambika VST

Software-Nachbau des **Mutable Instruments Ambika** (6-stimmiger hybrider Polysynth) als VST3/AU-Plugin für macOS (Universal Binary: Apple Silicon + Intel). Läuft auch als Standalone-App und unter Linux (VST3).

> Inoffizielles Community-Projekt, nicht mit Mutable Instruments verbunden. Grundlage ist die quelloffene Ambika-Firmware von Emilie Gillet (GPL v3).

## Was ist originalgetreu, was ist modelliert?

| Bereich | Umsetzung |
|---|---|
| **Voicecard-Digitalteil** (Oszillatoren, Sub-Osc, Transienten, Mixer, Crossmod, Fuzz, Crush, 3 Hüllkurven, Voice-LFO, Modulationsmatrix, Modifier, Portamento) | **Zeile für Zeile aus der Firmware portiert.** 8-Bit-Festkomma-Arithmetik bitgenau wie auf dem AVR (inkl. 16-Bit-`int`-Überläufen), Original-Wavetables und Lookup-Tabellen, 39 215,686 Hz Samplerate (20 MHz / 510), 40-Sample-Blöcke (~980 Hz Kontrollrate). |
| **Controller/Mainboard** (Voice-Allocation Mono/Poly/2× Unison/Cyclic, LFO 1–3 inkl. Clock-Sync, Arpeggiator mit 22 Patterns, 2 Step-Sequenzen + Notensequenz, Ragas/Mikrotunings, Groove-Templates) | **Aus der Firmware portiert.** |
| **DAC** | 12-Bit-DAC mit Zero-Order-Hold bei 39,2 kHz, inkl. „Crush“ (der Mikrocontroller schreibt nur jeden n-ten Wert). |
| **Analoge Filterkarten** | **Modelliert** (Zero-Delay-Feedback, 2× Oversampling = 78,4 kHz) nach den Ambika-Voicecard-Schaltplänen: **SMR4** (LM13700-OTA-Kaskade, 4-Pol-Tiefpass, OTA-VCA), **4P** (SSM2164, 4-Pol-Multimode LP/BP/HP), **SVF** (SSM2164, 2-Pol LP/BP/HP über 4052-Multiplexer). |
| **Analoge Eigenheiten** | RC-geglättete PWM-Steuerspannungen, VCA-Update mit einem Block Verzögerung wie auf der Hardware, Bauteiltoleranzen pro Karte (Cutoff, Resonanz, Pegel), Rauschen, Ausgangs-Koppelkondensator, weich sättigende Summierstufe. |

Die Filter-Modelle sind nach Gehör und Schaltung abgestimmt, nicht gegen eine echte Ambika gemessen. Die Charakteristik ist plausibel, Resonanzverlauf und Cutoff-Skalierung können aber von einem bestimmten Gerät abweichen.

## Installation (Mac)

### Variante A: fertig bauen lassen (empfohlen, kein Xcode nötig)

1. Dieses Projekt in ein eigenes GitHub-Repository hochladen.
2. Unter **Actions** läuft automatisch der Workflow „Build“ (ca. 10–15 Minuten).
3. Im fertigen Lauf das Artefakt **AmbikaVST-macOS** herunterladen und entpacken.
4. Kopieren:
   - `Ambika VST.vst3` → `~/Library/Audio/Plug-Ins/VST3/`
   - `Ambika VST.component` → `~/Library/Audio/Plug-Ins/Components/` (für Logic/GarageBand)
5. Da das Plugin nicht von Apple notarisiert ist, einmal im Terminal ausführen:
   ```
   xattr -cr ~/Library/Audio/Plug-Ins/VST3/"Ambika VST.vst3"
   xattr -cr ~/Library/Audio/Plug-Ins/Components/"Ambika VST.component"
   ```
6. DAW neu starten bzw. neu scannen lassen.

### Variante B: selbst bauen

Voraussetzungen: Xcode (aus dem App Store) und CMake (`brew install cmake`).

```
cmake -B build -G Xcode
cmake --build build --config Release --target AmbikaVST_VST3 AmbikaVST_AU AmbikaVST_Standalone
```

JUCE 8 wird beim ersten Konfigurieren automatisch heruntergeladen. Die fertigen Plugins liegen unter `build/AmbikaVST_artefacts/Release/`.

## Bedienung

Die Oberfläche folgt der Hardware: **40×2-LCD**, darunter **8 Potis**, darunter **8 Seitentasten**. Ein **erneuter Druck** auf die aktive Taste schaltet durch die Unterseiten (Punkte unter der Taste). Poti 1–4 gehören zur oberen LCD-Zeile, Poti 5–8 zur unteren. Beim Drehen zeigt die Titelzeile den vollen Parameternamen und Wert.

| Taste | Unterseiten | Inhalt |
|---|---|---|
| OSC | – | Osc 1 und 2: Waveform, Parameter, Range, Tune |
| MIXER | – | Mix, Crossmod (sync, ringmod, xor, fold, bits), Amount, Sub-Waveform, Sub, Noise, Fuzz, Crush |
| FILTER | – | Frequenz, Resonanz, Mode, Env2→VCF, LFO2→VCF, Voicecard-Typ, Drive |
| ENV/LFO | 3 | Hüllkurve 1–3 (ADSR) mit jeweils LFO 1–3 (Wave, Rate inkl. Clock-Teilungen, Trigger free/slave/master) |
| MATRIX | 7 | 14 Modulations-Slots (Source, Destination, Amount); Slot 14 wird vom Modwheel skaliert |
| MODIF. | 3 | 4 Modifier (add, prod, attn, max, min, xor, ≥, ≤, qtz, lag) und Voice-LFO (LFO 4) |
| PART | 3 | Part (Volume, Octave, Tune, Spread, Raga, Legato, Portamento, Polyphonie-Modus) · Arp/Seq · Clock/Setup |
| SEQ | – | Grafischer Editor für Seq 1, Seq 2 und Notenpattern (Balken ziehen; G/L-Reihe: Gate/Legato) |

Rechts: Presets (16 Werks-Presets, Laden/Speichern von `.ambk`-Patchdateien im Ambika-Speicherlayout, Randomize), Voicecard-LEDs, Wahl der Filterkarte, Output, Stereo-Spread, Rauschen, Anzahl Voicecards (1–6).

**Clock:** Standard ist „host“: Arpeggiator, Sequencer und synchronisierte LFOs laufen zum DAW-Tempo (24 ppqn) und starten mit dem Transport. Bei gestopptem Transport bzw. mit „internal“ startet die interne Clock wie auf der Hardware mit der ersten Note und stoppt nach der „Latch“-Zeit.

**MIDI:** Modwheel, Breath (CC 2, „whl2“), Expression (CC 4, „pdal“), Sustain, Pitchbend, Channel- und Poly-Aftertouch. Die CC-Zuordnungen der Ambika (z. B. CC 74 Cutoff, CC 71 Resonanz, CC 16–21 Oszillatoren) werden unterstützt. Alle Parameter sind in der DAW automatisierbar.

## Unterschiede zur Hardware

- **Ein Part pro Instanz.** Die Ambika ist 6-fach multitimbral; im Plugin entspricht das sechs Instanzen mit jeweils eigener Voicecard-Anzahl.
- **Chain-Modus** verhält sich wie Poly (es gibt keine zweite Ambika zum Verketten).
- Keine SD-Karte, kein SysEx-Import von Original-Dumps, keine Performance-Seite. Die `.ambk`-Dateien nutzen aber exakt das Byte-Layout von Patch (112 Bytes) und Part (84 Bytes).
- Die Hardware rendert LFO 1–3 bei ~976 Hz auf dem Mainboard, das Plugin bei der Voicecard-Blockrate (~980 Hz). Der Unterschied ist nicht hörbar.
- Synchronisierte LFO-Rate „1/96“: In der Firmware läuft der Phasenwert über (Wert 65536 in 16 Bit), das Plugin nutzt 65535.

## Projektstruktur

```
src/engine/   Portierte Firmware (voicecard.cc, part.cc), Tabellen, Analogmodelle, Engine
src/plugin/   JUCE-Plugin: Parameter, Prozessor, Oberfläche, Presets
tools/        render_test (Engine → WAV), preset_test (alle Presets durch das Plugin),
              gen_resources.py (Konvertierung der Firmware-Tabellen)
```

Tests unter Linux/macOS:
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target ambika_render_test ambika_preset_test
./build/ambika_render_test .
```

## Lizenz

GPL v3 (siehe `LICENSE`), wie die Original-Firmware. Firmware, Wavetables und Lookup-Tabellen: Copyright 2011–2012 Emilie Gillet. „Ambika“ und „Mutable Instruments“ sind Namen bzw. Marken ihrer Inhaber; dieses Projekt ist nicht offiziell.
