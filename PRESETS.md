# Presets: Quellen und Credits

Das Plugin enthält 1074 Programme, Duplikate sind bereits entfernt. Jedes Programm besteht aus Patch (112 Bytes) und Part-Daten (84 Bytes: Polyphonie, Arpeggiator, Sequenzen), genau wie auf der Ambika. Erzeugt werden sie mit `tools/import_presets.py`.

| Bank | Anzahl | Quelle |
|---|---|---|
| Ambika VST | 16 | Eigene Demo-Presets dieses Projekts |
| Factory A | 128 | Golden Card der Ambika (Werks-SD-Karte), [pichenettes/ambika](https://github.com/pichenettes/ambika) `controller/data/goldencard`, GPL v3 |
| Factory B | 22 | dito |
| Factory F | 80 | dito |
| Factory Multis | 7 | Einzelne Parts aus der Multi-Bank der Golden Card (dito) |
| Factory (early) | 128 | Frühere Fassung der Werksbank, `controller/data/programs` (dito) |
| FunkyFli, ludotex, Nedavine, bjarne, dzidek, jjuhkam, toneburst, fuse, c_04 Dance Evolution | 324 | Community-Patches aus der Sammlung [joegiralt/ambika-shruthi-instruments](https://github.com/joegiralt/ambika-shruthi-instruments) |
| Shruthi FunkyVoks, TubeOhm, FunkyFli, Misc | 369 | Für die Ambika konvertierte Shruthi-Bänke (Konvertierung: bjarne), aus derselben Sammlung |

Bank S der Golden Card ist identisch mit Bank A und deshalb nicht doppelt enthalten. Aus den Multi-Dateien (`.MUL`) wird jeder der 6 Parts als eigenes Programm übernommen. Ihre Namen enden auf die Part-Nummer, z. B. „Drumkit 01 3“.

**Hinweise**
- Die Community-Patches wurden von ihren Autorinnen und Autoren frei verteilt. Die Sammlung, aus der sie stammen, hat aber keine ausdrückliche Lizenz. Wer dieses Repository öffentlich macht, sollte das berücksichtigen.
- Viele Programme wurden auf Ambikas mit SVF- oder gemischten Filterkarten entworfen. Wenn ein Programm dünn oder anders als erwartet klingt, lohnt sich ein Wechsel der Filterkarte (SMR4 / 4P / SVF).
- Einige Patches nutzen Arpeggiator, Step-Sequencer oder Drum-Splits (Multi-Parts). Die erklingen erst mit laufender Clock bzw. auf bestimmten Tasten.
- Patches für die Fremd-Firmware „YAM“ können Werte enthalten, die die Original-Firmware nicht kennt. Solche Werte werden auf den gültigen Bereich begrenzt.
