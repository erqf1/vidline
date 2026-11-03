# Vidline

A fast video and music player with editing built in. Open a video (or a music file), cut it, change the speed of parts, blur faces or text, draw on it, add shapes, drop in images, videos and music, and export.

- **Also a good video and music player.** Make it your default app for mp4/mov/mkv and mp3/wav/flac: instant start, live frames while scrubbing, controls that hide by themselves in fullscreen, hold `Space` for 2× speed, click to pause, 5-second skip, ambient light around the video.
- **Music with its cover.** Songs fill the window with their blurred cover, and the Tags button lets you change cover, title, artist, album, year, track and genre without touching the sound.
- **Plugins, starting with Music.** Install the Music plugin in the settings for a library next to the player: songs, albums, artists, search, playlists, a queue with shuffle and repeat, media keys, and downloading the sound from a link with title and cover (via [yt-dlp](https://github.com/yt-dlp/yt-dlp), fetched on first use). Only download what you are allowed to.
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

## License

Apache License 2.0 for the Vidline source code. Release packages bundle [FFmpeg](https://ffmpeg.org) (GPL build) and [Qt](https://www.qt.io) (LGPLv3), which keep their own licenses.

Bundled fonts from Google Fonts: Anton, Bangers, Bebas Neue, Creepster, Lobster, Pacifico, Press Start 2P (SIL Open Font License 1.1) and Permanent Marker (Apache License 2.0).
