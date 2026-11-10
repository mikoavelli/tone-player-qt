# Build commands for tone-player-qt.
# Presets live in CMakePresets.json: release, debug, san.
# Every preset treats compiler warnings as errors.
# Windows: run from the "x64 Native Tools" prompt (or after vcvars64.bat).

set positional-arguments

presets := "release debug san windows"
build_dirs := "build build-dev build-san build-win"

# Default: optimized release build.
default: release

# Configure + build a preset (release|debug|san|windows).
build preset="release":
    @just --justfile {{justfile()}} _check_preset {{preset}}
    cmake --preset {{preset}}
    cmake --build --preset {{preset}} --parallel

# Cross build of a self-contained Windows TonePlayer.exe via cmake/mingw-static.cmake (needs ownstuff static Qt packages).
windows-static:
    cmake --preset windows
    cmake --build --preset windows --parallel
    echo "built: build-win/TonePlayer.exe (self-contained: Qt + deps static)"

# Assemble the release bundle in dist/: patched CKM2PO.exe + TonePlayer.exe + settings.json + the CKM2PO UsbDrv.dll proxy.
dist ckm2po_repo="../CKM2PO":
    #!/usr/bin/env bash
    set -euo pipefail
    just --justfile {{justfile()}} windows-static
    mkdir -p dist
    cp build-win/TonePlayer.exe dist/TonePlayer.exe
    cp settings.json dist/settings.json
    cp "{{ckm2po_repo}}/analysis/tonebutton/UsbDrv.dll" dist/UsbDrv.dll
    # patched CKM2PO: family-1 device-match relax (VM-verified, see analysis/patched/CKM2PO-variantA.exe)
    cp "{{ckm2po_repo}}/analysis/patched/CKM2PO-variantA.exe" dist/CKM2PO.exe
    echo "dist/: CKM2PO.exe TonePlayer.exe settings.json UsbDrv.dll"

# Optimized release build (the shippable one).
release:
    @just --justfile {{justfile()}} build release

# Development build for debugging.
debug:
    @just --justfile {{justfile()}} build debug

# Debug build with AddressSanitizer + UndefinedBehaviorSanitizer.
san:
    @just --justfile {{justfile()}} build san

# Build every Linux preset in turn (not the Windows cross build).
all:
    #!/usr/bin/env bash
    set -euo pipefail
    for preset in release debug san; do
        echo ":: $preset"
        just --justfile {{justfile()}} build "$preset"
    done

# Release build in another language, e.g. just lang ru.
lang language:
    cmake --preset release -DTONE_PLAYER_LANGUAGE={{language}}
    cmake --build --preset release --parallel

# Build and run a preset (the default preset is release).
run preset="release":
    #!/usr/bin/env bash
    set -euo pipefail
    just --justfile {{justfile()}} build "{{preset}}"
    case "{{preset}}" in
        release) dir=build ;;
        debug) dir=build-dev ;;
        san) dir=build-san ;;
        *) echo "unknown preset: {{preset}}" >&2; exit 2 ;;
    esac
    cd "{{source_directory()}}"
    exec "./$dir/tone-player-qt"

# Headless smoke run of the release build (no window, 5 s).
smoke:
    #!/usr/bin/env bash
    set -euo pipefail
    just --justfile {{justfile()}} build release
    cd "{{source_directory()}}"
    timeout 5 env QT_QPA_PLATFORM=offscreen ./build/tone-player-qt >/dev/null 2>&1 || code=$?
    if [ "${code:-0}" -eq 124 ]; then
        echo "smoke: ok (kept running for 5 s)"
    else
        echo "smoke: FAILED (exit ${code:-0})" >&2
        exit 1
    fi

# Put settings.json next to the binaries of every configured preset.
settings:
    #!/usr/bin/env bash
    set -euo pipefail
    for dir in {{build_dirs}}; do
        if [ -d "$dir" ]; then
            cp settings.json "$dir/settings.json"
            echo "copied -> $dir/settings.json"
        fi
    done

# Remove all preset build directories.
clean:
    #!/usr/bin/env bash
    set -euo pipefail
    rm -rf {{build_dirs}}
    echo "cleaned: {{build_dirs}}"

# Generate HTML API documentation into build/doxygen/html.
docs:
    #!/usr/bin/env bash
    set -euo pipefail
    cd "{{source_directory()}}"
    doxygen Doxyfile
    echo "docs: build/doxygen/html/index.html"

# Windows release: build, then deploy the Qt runtime into build\package.
windows package_dir="build/package":
    cmake --preset release
    cmake --build --preset release --parallel
    windeployqt --release --no-translations --dir "{{package_dir}}" build/tone-player-qt.exe
    cp settings.json "{{package_dir}}/settings.json"

# Show available commands.
list:
    @just --list

# --- internals ---------------------------------------------------------------

_check_preset preset:
    #!/usr/bin/env bash
    set -euo pipefail
    case " {{presets}} " in
        *" $1 "*) ;;
        *) echo "unknown preset: $1 (expected: {{presets}})" >&2; exit 2 ;;
    esac
