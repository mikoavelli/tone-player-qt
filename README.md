# tone-player-qt

A native C++ / Qt 6 port of **tone-player** — a multitone audio equalizer player.

## Overview

The application generates a sequence of sine tones using the levels of an
11-band equalizer, plays them through an output device of your choice, and
persists all settings in `settings.json` next to the executable.

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
just clean        # remove all preset build directories
```

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
embedded into the binary):

```sh
cmake -S . -B build -DTONE_PLAYER_LANGUAGE=ru   # requires resources/i18n/ru.json
```

Currently only `en.json` is present; a Russian build just needs a
`resources/i18n/ru.json` with the same keys.

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
