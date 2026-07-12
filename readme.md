# breakdance

A meme program which play Breakdance in loop.

<center>
<iframe width="887" height="887" src="https://www.youtube.com/embed/kT3Wm8Nhapw" title="A wrong CD inserted" frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" referrerpolicy="strict-origin-when-cross-origin" allowfullscreen></iframe>
</center>

---

A C++ video looper that ships as a **single binary** — video baked in, no external files needed at runtime.

- Video embedded directly in the executable at build time
- Window title embedded from `assets/title.txt`
- Borderless window with a custom title bar, custom border, and bottom status bar
- Audio playback from the embedded media
- No playback controls — just looping video
- SDL2 + FFmpeg, C++17

---

## Build guide

### Prerequisites

| Tool | Notes |
|------|-------|
| CMake ≥ 3.20 | cmake.org |
| C++17 compiler | MSVC 2022, GCC ≥ 11, or Clang ≥ 13 |
| SDL2 + FFmpeg dev libs | via apt (Linux) or vcpkg (Windows / Linux) |

### Linux / WSL2

#### Options

```
./build.sh [OPTIONS]

  --deps              Install system dependencies via apt then exit
  --build             Configure and compile
  --jobs N            Parallel compile threads (default: all logical cores)
  --video FILE        Embed FILE as assets/video.mp4 before building
  --vcpkg DIR         Use vcpkg at DIR instead of apt packages
  --help              Show help
```

#### First-time setup (system packages — recommended for WSL2)

```bash
chmod +x build.sh

# Install deps + build + embed video in one shot
./build.sh --deps --build --video /path/to/your_video.mp4

# Run
./build/breakdance
```

#### Common workflows

```bash
# Install dependencies only
./build.sh --deps

# Build with all cores (after deps are installed)
./build.sh --build

# Build with 4 threads
./build.sh --build --jobs 4

# Swap in a new video and rebuild
./build.sh --build --video /path/to/new_video.mp4

# Use vcpkg instead of apt
./build.sh --deps --build --vcpkg ~/vcpkg --video /path/to/your_video.mp4
```

#### WSL2 display

WSL2 with **WSLg** (Windows 11 / Windows 10 21H2+) works out of the box.

Older WSL2 without WSLg needs an X server (e.g. VcXsrv or Xming) on the Windows side:

```bash
export DISPLAY=$(grep nameserver /etc/resolv.conf | awk '{print $2}'):0
./build/breakdance
```

### Windows

#### Options

```
build.bat [OPTIONS]

  --deps              Install vcpkg packages then exit
  --build             Configure and compile
  --jobs N            Parallel compile threads (default: all logical cores)
  --video FILE        Embed FILE as assets\video.mp4 before building
  --vcpkg DIR         Path to vcpkg root (or set VCPKG_ROOT env var)
  --help              Show help
```

#### First-time setup

```bat
REM Bootstrap vcpkg once
git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat

REM Install deps + build + embed video in one shot
build.bat --deps --build --video C:\path\to\your_video.mp4 --vcpkg C:\vcpkg

REM Run
build\Release\breakdance.exe
```

#### Common workflows

```bat
REM Install dependencies only
build.bat --deps --vcpkg C:\vcpkg

REM Build with all cores
build.bat --build --vcpkg C:\vcpkg

REM Build with 4 threads
build.bat --build --jobs 4 --vcpkg C:\vcpkg

REM Swap in a new video and rebuild
build.bat --build --video C:\path\to\new_video.mp4 --vcpkg C:\vcpkg
```

### Updating embedded assets

Update either asset and rebuild:

- `assets/video.mp4` — the video content
- `assets/title.txt` — the window title shown in the OS window and custom title bar

For the video, you can also pass `--video` on the next build. Asset embedding is incremental: if `assets/video.mp4` and `assets/title.txt` are unchanged, rebuilds reuse the generated headers instead of re-embedding them.

Manual rebuild:

```bash
# Linux
cmake --build build --target embed_video
cmake --build build --target embed_title
cmake --build build --config Release
```

```bat
REM Windows
cmake --build build --target embed_video
cmake --build build --target embed_title
cmake --build build --config Release
```

`cmake/embed_video.cmake` reads `assets/video.mp4` and writes `src/video_data.h`.
`cmake/embed_title.cmake` reads `assets/title.txt` and writes `src/title_data.h`.
Both assets are compiled into the executable, so no external files are needed at runtime.

---

### Window controls

| Action | Effect |
|--------|--------|
| Drag title bar | Move window |
| Click collapse | Hide/show the video area |
| Click size | Toggle between normal and small size |
| Click **×** | Quit |
| `Escape`/`Return`/`Enter`/`q` | Quit (emergency exit) |

### Supported video formats

Any format FFmpeg can decode: MP4 (H.264/H.265), WebM (VP8/VP9), MKV, AVI, MOV, etc.

## DISCLAIMER

Since this is a meme project, I do not pay much effort. This project contains AI-generated code, its development is ***dead by birthtime*** and no maintainance will be made. However, `pull requests` are welcomed, just remember to include your build binary with Virus Total scan result.
