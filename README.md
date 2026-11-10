# tone-player-qt

A native C++ / Qt 6 port of **tone-player** — a multitone audio equalizer player.

## Overview

The application generates a sequence of sine tones using the levels of an
11-band equalizer, plays them through an output device of your choice, and
persists all settings in `settings.json` next to the executable.

A minimal named-pipe IPC server (`\\.\pipe\tone-player`, same wire format as the
Python original) accepts one-line calls and answers one line of JSON
`{"ok": ..., "message": "..."}`: `status`, `play [index]`, `stop`, `toggle`
(`toggle` = playing/paused → stop, otherwise start from the focused band — this is
what the CKM2PO TONE button uses). Remote calls never raise dialogs.

All user-facing strings are compiled into the binary from a Qt resource
(`resources/i18n/en.json`), so the program only ships as
**one executable + `settings.json`** — no external .qm files or data folders.

## Requirements

- C++23 compiler (GCC 11+ / Clang 14+)
- Qt 6 (6.12 recommended) with the **Qt Multimedia** module
- CMake 3.21+

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
```

The produced `build/tone-player-qt` binary is self-contained:
copy it anywhere and put a `settings.json` beside it.

### Shortcuts (`just`)

`CMakePresets.json` holds three configurations — `release` (`-O2` + LTO),
`debug`, and `san` (Debug + ASan/UBSan). All of them treat compiler warnings
as errors. The `justfile` wraps them:

```sh
just              # optimized release build
just debug        # development build
just san          # debug build with sanitizers
just all          # build all presets
just run [preset] # build and run (release by default)
just smoke        # headless 5-second run of the release build
just settings     # copy settings.json into every build directory
just lang ru      # release build with another compiled-in language
just windows      # Windows: build + windeployqt into build/package
just windows-static  # Windows cross build (self-contained exe) → build-win/
just dist         # assemble dist/ release: CKM2PO.exe (patched) + TonePlayer.exe + settings.json + UsbDrv.dll
just clean        # remove all preset build directories
```

### Windows release bundle

The deliverable is a **4-file set** in `dist/`:

| File | What it is |
|------|------------|
| `CKM2PO.exe` | Patched СКМ2ПО (variant A: device-match relax, 4 NOP'd JNZ guards in `FUN_00589240`; no serial/description gating). Deployed and verified on the Win10 test VM; the empty-editor build host is `~/Projects/CKM2PO/analysis/patched/CKM2PO-variantA.exe`, sha256 `fca2b18b70536ab27032aaa1762e2b5e875626173402419f550cf238e1653aab`. |
| `UsbDrv.dll` | The СКМ2ПО FTDI proxy (source `analysis/tonebutton/tonebtn.c`): forwards the 8 genuine FT_* exports to the `UsbDrv_orig.dll` you rename the stock one to, and paints the APP (open TonePlayer) and TONE (play/stop) buttons on the toolbar. |
| `TonePlayer.exe` | The Qt port itself — self-contained: Qt and all dependencies are statically linked, no Qt DLLs needed. |
| `settings.json` | Tones 1..20 (500 Hz..10000 Hz, 10 s each). |

Installation on a Windows host (installs into the СКМ2ПО directory, e.g.
`C:\CKM2PO\`):

1. Stop СКМ2ПО; rename the stock `UsbDrv.dll` → `UsbDrv_orig.dll`.
2. Copy the four files from `dist/` next to `CKM2PO.exe` (they replace it).
3. Start СКМ2ПО; check the APP and TONE buttons top-right, and `tonebtn.log`
   next to `UsbDrv.dll` for `ini absent -> default TonePlayerExe=` — the proxy takes
   the exe location next to itself, no `.ini` required.

`just windows-static` cross-compiles the bundle with MinGW-w64 against the static
Qt 6.12 packages from Ark's `ownstuff` repository
(`ownstuff/mingw-w64-qt6-base-static`, `ownstuff/mingw-w64-qt6-multimedia-static`);
`cmake/mingw-static.cmake` plus `cmake/pkg-config-static.sh` and
`cmake/compat/unofficial-brotli/` force the static archives of every third-party
dependency so no MinGW runtime DLL is imported. See `just/dist` for the full
bundle assembly.

CMake can also be driven directly with the same presets:
`cmake --preset release && cmake --build --preset release`.

### Build options

| Option | Default | Meaning |
|--------|---------|---------|
| `CMAKE_BUILD_TYPE` | — | `Debug` for development, `Release` for shipping |
| `TONE_PLAYER_LANGUAGE` | `en` | UI language compiled into the binary |
| `TONE_PLAYER_LTO` | `ON` | Interprocedural (link-time) optimization in release builds |
| `TONE_PLAYER_NATIVE_OPT` | `OFF` | `-march=native` (GCC/Clang); the binary stops being portable |
| `TONE_PLAYER_WARNINGS_AS_ERRORS` | `ON` | Fail the build on any compiler warning |
| `TONE_PLAYER_SANITIZERS` | `OFF` | ASan + UBSan in Debug builds (GCC/Clang only) |

### Windows (primary target)

Configure with the MSVC toolchain — strict MSVC flags (`/W4`, `/permissive-`,
`/utf-8`, `/sdl`) are enabled automatically; `WIN32_EXECUTABLE` keeps the
console subsystem off. Suggested release config:

```sh
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Linux (development, maintenance, debugging)

The full GCC/Clang strict-diagnostics set and all optimizations remain
available; nothing is Windows-only in the build system.

```sh
# Development / debugging
cmake -S . -B build-dev -DCMAKE_BUILD_TYPE=Debug
cmake --build build-dev -j8

# Bug hunting with sanitizers
cmake -S . -B build-san -DCMAKE_BUILD_TYPE=Debug -DTONE_PLAYER_SANITIZERS=ON
cmake --build build-san -j8
```

Headless smoke run: `QT_QPA_PLATFORM=offscreen ./tone-player-qt`.

## Language

The interface language is chosen at **compile time** (all translations are
embedded into the binary). `resources/i18n/en.json` and
`resources/i18n/ru.json` are both present and carry the same set of keys:

```sh
cmake -S . -B build -DTONE_PLAYER_LANGUAGE=ru
cmake --build build
# or: just lang ru
```

## Runtime notes

- `settings.json` is looked up next to the executable (or beside `argv[0]`);
  it is mandatory — a missing file means the program is launched without its
  configuration.
- If the settings file is corrupted, the application quarantines it as
  `settings.corrupted-<timestamp>.json` and continues with the file contents
  recovered where possible.
- `settings.json` is watched for external edits and reloaded automatically.
- Levels are written with Python-style float formatting so the file stays
  byte-compatible with the original Python version of the app.

## Project layout

```
CMakeLists.txt            Build configuration (language option, resources, sources)
resources/i18n/en.json    All UI strings, embedded via the Qt resource system
settings.json             User settings (next to the executable at runtime)
src/                      Domain logic: models, settings store, presets,
                          bands/equalizer math, audio engine (QAudioSink),
                          text formatting, i18n runtime
src/ui/                   Widgets: level sliders, ruler, band columns,
                          settings dialog, main window
```

---

## License

This project is open-source and released under the [MIT License](LICENSE).
