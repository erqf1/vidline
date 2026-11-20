ERGEBNIS CLOUD-SESSION vidline-117

1. Aufgabe und Status
   Vidline 1.17.0 mit automatischen Untertiteln (whisper.cpp-Plugin), mehreren Spuren und Export-Vorlagen bauen,
   testen und veröffentlichen. Status: fertig (Release-Workflow läuft beim Schreiben, Stand unten)

2. Branch, Commits, PR, Release
   - Branch: claude/festive-euler-nxo1y9 (ab main bb2da44)
   - Code-Stand mit grünen Tests: d30f17d; Release-Commit: dieser Commit „Vidline 1.17.0 …“
   - PR nach main: https://github.com/erqf1/vidline/pull/1 (nicht gemergt – Entscheidung des Nutzers)
   - Tag: v1.17.0 auf dem Release-Commit des Branches (nicht auf main)
   - CI: Workflow „Tests“ grün auf Ubuntu 24.04, Windows, macOS 15 (Lauf 6 auf d30f17d);
     Release-Probelauf per workflow_dispatch grün (Windows, Linux, Arch, macOS Intel + Apple Silicon, whisper-cli für alle 4)
   - Release: https://github.com/erqf1/vidline/releases/tag/v1.17.0 (Ergebnis wird nachgetragen)

3. Was geändert wurde
   - Untertitel-Plugin (src/subtitles.*): Einstellungen → Plugins → Untertitel. whisper-cli + Modell (Tiny/Base/Small/
     Turbo/Medium) erst bei Benutzung geladen, mit Fortschritt; Sprache auto oder wählbar, optional „ins Englische“.
     Modelle von huggingface.co/ggerganov/whisper.cpp. whisper-cli baut der Release-Workflow aus dem offiziellen
     whisper.cpp-Quellcode (Tag v1.9.5, statisch) und legt es als whisper-cli.<system>.zip/.tgz ins Release – ggml-org
     liefert keine Linux/macOS-Programme. Namen so gewählt, dass der Updater alter Versionen sie NIE für ein Update hält;
     der neue Updater schließt „whisper*“ zusätzlich aus.
   - Untertitel-Spur in der Zeitleiste (verschieben, kürzen, teilen, löschen, Rückgängig), Eigenschaften: Text, Zeiten,
     Schrift, Größe, Position, Rand/Kasten/Schatten, Farben; Vorschau im Bild. SRT/VTT importieren/speichern.
     Export: einbrennen (eigene PNG-Bilder + ffconcat, kein libass nötig), als Untertitelspur (mov_text/srt), SRT daneben.
   - Mehrere Spuren (model.h Track/Item::Video, timeline.*, mainwindow.cpp): V1 = Hauptvideo, V2… Bild-im-Bild/Overlays
     (Position, Größe, Deckkraft, eigener Ton), A1… Ton-Spuren; Spurköpfe mit M/S/Lautstärke + „+“, Spur-Eigenschaften,
     Clips per Ziehen/Rechtsklick zwischen Spuren (auch Abschnitt ↔ Overlay), Rechtsklick im Medien-Panel.
     Export: ein filter_complex mit allen Spuren, Stumm/Solo/Lautstärke wirken.
   - Export-Vorlagen (src/presets.*, dialogs.cpp): TikTok/Reels/Shorts 9:16 (Rand/Zuschnitt/unscharfer Rand), YouTube
     1080p/4K, Discord <10 MB und <50 MB (Bitrate aus Länge, 2-Pass, Auflösung passt sich an), Instagram 1:1, GIF,
     eigene Vorlagen speichern/löschen; Vorschau der Zielgröße (Rahmen) + Dateigröße.
   - Musikbibliothek: ein Klick spielt den Song (Wunsch während der Session); Strg/Umschalt wählt weiter mehrere.
   - Fehler behoben: Rückgängig nach Änderungen über vorher geholte Zeiger (Snapshot teilte Daten), Absturz beim Beenden
     (onFrame nach Abbau), Eigenschaften breiter als ihr Bereich, Unschärfe-Farbe orange → grau-blau (kein Orange neben Grün).
   - 91 neue Texte in allen 12 Sprachen; README; Version 1.17.0; packaging/release-notes.md (kurz, Deutsch, Link changelog).
   - Workflows: .github/workflows/tests.yml (neu), release.yml (whisper-cli, Prüfung „alle Pakete da“, deutsche Notizen).
   - Wichtige Dateien: src/subtitles.*, src/presets.*, src/selftests.cpp, src/model.*, src/timeline.*, src/mainwindow.*,
     src/dialogs.*, src/i18n.cpp, src/updater.cpp, src/music.*, website-updates/, .github/workflows/*.

4. Getestet
   - --trackstest 33/33, --presettest 48/48, --subtest 45/45 (offline) – lokal (Linux) und in CI auf Linux, Windows, macOS.
     Enthält echte Exporte je Vorlage (Auflösung, Länge, Dateigröße), 2-Pass-Zielgröße (≤ 1 MB, 77 % genutzt),
     Pixel-/Tonband-Prüfungen der Spur-Mischung, eingebrannte Untertitel im Bild, Untertitelspur in MP4/MKV,
     unveränderte ffmpeg-Befehle für Alt-Projekte (Golden-Vergleich mit 1.16).
   - --subtest --online lokal 48/48: Modell „tiny“ von Hugging Face geladen (mit Fortschritt), gesprochener Testsatz
     (ffmpeg/flite) korrekt erkannt: „Hello world, this is a short test of automatic subtitles.“
   - Programm-Download + Entpacken + Start von whisper-cli über einen lokalen Server mit Release-artigem Archiv getestet.
   - Alle drei neuen Tests zusätzlich unter AddressSanitizer ohne Fehler; --musicshots 39/39 (inkl. Ein-Klick), --voltest 14/14.
   - NICHT getestet: Download von whisper-cli direkt aus dem GitHub-Release (aus der Cloud gesperrt, Datei gibt es erst
     mit dem Release); echte Erkennung unter Windows/macOS; whisper-cli-Builds wurden in CI nur mit --help gestartet;
     Bedienung per echter Maus (Ziehen zwischen Spuren) nur über Code-Pfade/Tests, nicht von Hand.

5. Offen / Risiken / zu tun
   - PR nach main mergen (Release wurde vom Branch getaggt; main ist bis zum Merge noch 1.16).
   - Website: website-updates/changelog-1.17.0.html in changelog.html, features-1.17.0.html auf mert.win/vid/,
     website-updates/img/*.webp nach mert.win/vid/img/ hochladen (Anleitung: website-updates/LIESMICH.md).
   - Nach dem Release einmal unter Windows/macOS: Plugin „Untertitel“ installieren, Untertitel erzeugen (lädt whisper-cli
     aus dem Release) – prüfen, dass Gatekeeper/Defender nichts blockieren (macOS: ad-hoc signiert).
   - Zwischen Spuren verschobene Abschnitte: das Hauptvideo rückt zusammen (keine Lücken in V1).
   - Linux aarch64/Windows ARM: kein whisper-cli (Plugin meldet das).

6. Nächste Schritte für die Haupt-Session
   - PR prüfen und mergen; Website-Schnipsel einbauen und Bilder hochladen.
   - Release-Seite ansehen (14 Dateien inkl. whisper-cli.*) und Auto-Update von 1.16 testen.
   - Ideen für 1.18: Wortgenaue Untertitel-Animation, Übergänge zwischen Clips, Keyframes für Overlays.
