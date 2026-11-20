# Vidline

A fast video and music player with editing built in. Open a video (or a music file), cut it, change the speed of parts, blur faces or text, draw on it, add shapes, drop in images, videos and music, and export.

- **Also a good video and music player.** Make it your default app for mp4/mov/mkv and mp3/wav/flac: instant start, live frames while scrubbing, controls that hide by themselves in fullscreen, hold `Space` for 2× speed, click to pause, 5-second skip, ambient light around the video.
- **Music with its cover.** Songs fill the window with their blurred cover, and the Tags button lets you change cover, title, artist, album, year, track and genre without touching the sound.
- **Plugins, starting with Music.** Install the Music plugin in the settings for a library next to the player (one click plays a song): songs, albums, artists, search, playlists, a queue with shuffle and repeat, media keys, and downloading the sound from a link with title and cover (via [yt-dlp](https://github.com/yt-dlp/yt-dlp), fetched on first use). Only download what you are allowed to.
- **Video download plugin.** Paste a link from YouTube, Instagram, TikTok, X, Twitch, Vimeo or 1000+ other sites, check it, then pick video with sound, video only or audio only, the real available quality (with fps and size), MP4/MKV/WebM or a most-compatible H.264 option, subtitles, just a section (e.g. 1:30–2:00) or a whole playlist. Downloads run in a queue; sign-in can be taken from your browser for private or age-restricted videos.
- **Automatic subtitles (plugin).** Install the Subtitles plugin in the settings: speech is recognized offline on your own computer with [whisper.cpp](https://github.com/ggml-org/whisper.cpp) (MIT; program and language model are downloaded on first use, with progress). Pick the model (Tiny … Medium) and the language or let it detect it. The subtitles sit on their own track – edit text and times, split, move – and are saved as SRT/VTT, embedded as a subtitle track or burned into the picture with your font, size, position and outline or background box.
- **Multiple tracks.** Stack video and image tracks (picture-in-picture, logos, overlays with position, size and opacity) and use several audio tracks for music and voice. Every track has volume, mute and solo; drag clips to another track or right-click them. The export mixes everything with one ffmpeg filter graph.
- **Export templates.** TikTok/Reels/Shorts (9:16 with black bars, crop or blurred bars), YouTube 1080p and 4K, Discord under 10 MB and 50 MB (bitrate from the length, two passes), Instagram 1:1, GIF – plus your own saved templates, with a preview of the target frame and file size.
- **One volume for everything** next to the time: slider, mute (click or M), arrow keys up/down. It is not saved, so every start begins at 100 %.
- **Opens instantly.** No import step, no project setup. Drag a video in and it plays – waveforms and thumbnails for editing are only computed when you start editing.
- **Edit mode with a timeline.** Drag segment edges to trim (the edge follows your mouse, the gap closes when you let go), drag segments to reorder them, split only what is selected (or everything at the playhead), per-segment speed, zoom in/out.
- **Resizable layout** like in CapCut: drag the dividers to make the media panel, the video, the properties or the timeline bigger or smaller.
- **Draw and shapes.** Paint on the video, add rectangles, circles, arrows and lines – with color, line width and fill.
- **Volume like in the big editors.** Waveforms on every clip; drag the volume line up or down and see the level in dB.
- **Resize, move and rotate** each segment, add **text** (bundled display fonts or any installed font, with or without background), image overlays and real blur areas.
- **Frame format** for any platform: original, 16:9, 9:16 (TikTok, Reels, Shorts), 1:1, 4:5, 4:3 or 21:9 – the video is fitted in, zoom it with the size slider to fill the frame.
- **Media panel.** Everything you import sits on the right; double-click to insert it at the playhead.
- **Sensible export.** MP4, MOV, MKV, GIF or audio only (MP3, WAV, M4A). Pick resolution (4K … SD), frame rate and quality – only values up to the original are offered.
- **Checks for updates** and installs them on request (or ignore a version).
- **Fullscreen** with `F11`, click the video to pause, arrow keys jump 5 seconds.
- **12 languages** and twelve themes: Mint, Studio, Ocean, Violet, Sunset, Light, the calm Minimal style (dark and light), Acrylic (smoke in your accent colour and matte glass, dark and light), Windows XP Luna and Terminal – plus any accent color you like.
- Uses your GPU encoder when available (NVENC, Quick Sync, AMF, VideoToolbox), otherwise x264.

## Download

Get the latest build from the [Releases page](../../releases/latest):

| Platform | File |
| --- | --- |
| Windows (x64) installer | `Vidline-windows-x64-setup.exe` – lets you choose the install folder and adds Vidline to the **Open with** menu of videos |
| Windows (x64) portable | `Vidline-windows-x64.zip` – unzip and run `Vidline.exe` |
| Linux (Debian / Ubuntu) | `vidline_amd64.deb` – `sudo apt install ./vidline_amd64.deb` |
| Linux (Arch) | `vidline-x86_64.pkg.tar.zst` – `sudo pacman -U vidline-x86_64.pkg.tar.zst` |
| Linux (any, tar.gz) | `vidline-linux-x86_64.tar.gz` – needs Qt 6 and ffmpeg from your distro |
| macOS Intel | `Vidline-macos-intel.dmg` |
| macOS Apple Silicon | `Vidline-macos-apple-silicon.dmg` |

macOS builds are not notarized. On first start, right-click the app and choose **Open**.

## Publishing (maintainers)

Run `publish.bat`: it asks for your GitHub username, opens the new-repository page, pushes the code and the `v1.0.0` tag (which triggers the release builds).

## Keyboard shortcuts

| Key | Action |
| --- | --- |
| `Space` / click video | Play / pause (hold `Space`: 2× speed) |
| `←` / `→` | Back / forward 5 s |
| `F11`, `Esc` | Fullscreen on / off |
| `E` | Edit mode |
| `S` | Split at playhead |
| `Q` / `W` | Trim start / end to playhead |
| `Del` | Remove selected segment or element |
| `B` / `I` | Add blur area / image |
| `D` / `R` | Draw / shape (`Esc` stops) |
| `Ctrl+O`, `Ctrl+E` | Open, export |
| `Ctrl+Z`, `Ctrl+Y` | Undo, redo |

## Build from source

Requires Qt 6.4+ (Widgets, Multimedia, MultimediaWidgets), CMake 3.21+ and a C++17 compiler. Vidline calls `ffmpeg` for thumbnails and export; put an `ffmpeg` binary next to the executable or on your `PATH`.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

On Windows, `build.bat` builds and creates the portable zip. Release packages for all platforms are built by GitHub Actions (`.github/workflows/release.yml`) when a `v*` tag is pushed.

## Developer tests

Hidden test modes (run with `QT_QPA_PLATFORM=offscreen`, results in `<folder>/*-ergebnis.txt`, exit code 1 on failure); they create their own test videos with ffmpeg. The Tests workflow runs the first three on Windows, Linux and macOS.

| Mode | Checks |
| --- | --- |
| `--trackstest <folder>` | tracks, moving clips, undo, mute/solo, a real mixed export (picture-in-picture, opacity, audio bands), unchanged export commands for old projects |
| `--presettest <folder>` | every template with a real export (resolution, length, file size), crop/blur bars, 2-pass size target, custom templates, export dialog |
| `--subtest <folder> [--online]` | SRT/VTT, times, whisper output, burn-in filter and real burn-in/subtitle-track export; with `--online` a real recognition of a spoken test sentence |
| `--voltest`, `--vdltest`, `--musicshots`, `--defaultsshots` | volume, video download, music library, default apps |

## License

Apache License 2.0 for the Vidline source code. Release packages bundle [FFmpeg](https://ffmpeg.org) (GPL build) and [Qt](https://www.qt.io) (LGPLv3), which keep their own licenses.

The Subtitles plugin downloads `whisper-cli`, built by the release workflow from the official [whisper.cpp](https://github.com/ggml-org/whisper.cpp) source (MIT License), and the official ggml models from Hugging Face.

Bundled fonts from Google Fonts: Anton, Bangers, Bebas Neue, Creepster, Lobster, Pacifico, Press Start 2P (SIL Open Font License 1.1) and Permanent Marker (Apache License 2.0).
