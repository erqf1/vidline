# Website-Updates für Vidline 1.17.0

Die Website (mert.win/vid) liegt nicht in diesem Repository. Diese Dateien sind fertig zum Einbauen:

| Datei | Wohin |
| --- | --- |
| `changelog-1.17.0.html` | `mert.win/vid/changelog.html`, oben über dem Eintrag 1.16.0 |
| `features-1.17.0.html` | `mert.win/vid/` – drei neue Abschnitte (Untertitel, Spuren, Vorlagen) und Galerie-Bilder |
| `img/*.webp` | nach `mert.win/vid/img/` hochladen |

Die Bilder stammen aus den Test-Modi (`--trackstest`, `--subtest`, `--presettest`, Design Mint, Sprache Deutsch)
und enthalten keine Benutzerpfade. Neu erzeugen:

```
VIDLINE_SETTINGS=<ordner> QT_QPA_PLATFORM=offscreen vidline --trackstest <ausgabe>
VIDLINE_SETTINGS=<ordner> QT_QPA_PLATFORM=offscreen vidline --subtest <ausgabe>
VIDLINE_SETTINGS=<ordner> QT_QPA_PLATFORM=offscreen vidline --presettest <ausgabe>
ffmpeg -i <ausgabe>/spuren-editor.png -c:v libwebp -quality 88 img/vid-117-spuren.webp
```

Klassen und Struktur der Schnipsel sind bewusst schlicht; bei Bedarf an die vorhandenen Abschnitte angleichen.
