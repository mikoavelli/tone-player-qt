cmake_minimum_required(VERSION 3.21)

project(tone-player-qt
    VERSION 1.0.0
    DESCRIPTION "TonePlayer in Qt: control tone playback with an equalizer"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Release presets promise -O2 regardless of the toolchain's default release
# level (some CMake versions/distributions default to -O3).
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    string(REGEX REPLACE "-O[0-9]+|-Os" "-O2" CMAKE_CXX_FLAGS_RELEASE
        "${CMAKE_CXX_FLAGS_RELEASE}")
endif()

# Language of the build: en.json / ru.json ... from resources/i18n is compiled
# into the binary. A release in another language is produced by changing this
# option; the layout of the deliverables does not change: binary + settings.json.
set(TONE_PLAYER_LANGUAGE "en" CACHE STRING "Interface language compiled into the binary")
find_file(TONE_PLAYER_I18N_FILE
    NAMES ${TONE_PLAYER_LANGUAGE}.json
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/resources/i18n
    NO_CACHE_VARIABLE
    NO_DEFAULT_PATH
)
if(NOT TONE_PLAYER_I18N_FILE)
    message(FATAL_ERROR "Translation resources/i18n/${TONE_PLAYER_LANGUAGE}.json not found")
endif()

find_package(Qt6 6.12 REQUIRED COMPONENTS Widgets Multimedia)
qt_standard_project_setup(REQUIRES 6.12)

set(TONE_PLAYER_SOURCES
    src/main.cpp
    src/i18n.h
    src/i18n.cpp
    src/errors.h
    src/models.h
    src/settings_store.h
    src/settings_store.cpp
    src/presets.h
    src/presets.cpp
    src/bands.h
    src/bands.cpp
    src/audio_engine.h
    src/audio_engine.cpp
    src/text_format.h
    src/text_format.cpp
    src/theme.h
    src/ui/main_window.h
    src/ui/main_window.cpp
    src/ui/band_widgets.h
    src/ui/band_widgets.cpp
    src/ui/settings_dialog.h
    src/ui/settings_dialog.cpp
)
qt_add_executable(tone-player-qt
    ${TONE_PLAYER_SOURCES}
)
target_include_directories(tone-player-qt PRIVATE src src/ui)

# Translation compiled into the binary: resource path inside a single json.
target_compile_definitions(tone-player-qt PRIVATE
    TONE_PLAYER_TRANSLATION=":/i18n/${TONE_PLAYER_LANGUAGE}.json"
)
set_property(SOURCE "${TONE_PLAYER_I18N_FILE}" PROPERTY QT_RESOURCE_ALIAS
    "${TONE_PLAYER_LANGUAGE}.json")
qt_add_resources(tone-player-qt toneplayer-i18n
    PREFIX "/i18n"
    FILES ${TONE_PLAYER_I18N_FILE}
)

target_link_libraries(tone-player-qt PRIVATE Qt6::Widgets Qt6::Multimedia)

set_target_properties(tone-player-qt PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE FALSE
)

# -----------------------------------------------------------------------------
# Warnings and optimizations
# -----------------------------------------------------------------------------

include(CheckIPOSupported)

option(TONE_PLAYER_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(TONE_PLAYER_LTO "Enable interprocedural (link-time) optimization" ON)
option(TONE_PLAYER_NATIVE_OPT "Optimize for the build machine (-march=native), non-portable" OFF)
# Development helper for Linux: TONE_PLAYER_SANITIZERS=ON adds AddressSanitizer
# and UndefinedBehaviorSanitizer (GCC/Clang only). Use it in a separate build
# directory, together with the Debug build type.
option(TONE_PLAYER_SANITIZERS "Build with ASan + UBSan (GCC/Clang only)" OFF)

# Strict diagnostics. GCC/Clang and MSVC have different spellings; the flags
# are applied only to our sources, never to Qt itself.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(TONE_PLAYER_WARNING_FLAGS
        -Wall
        -Wextra
        -Wpedantic
        # Conversion and narrowing.
        -Wconversion
        -Wsign-conversion
        -Wdouble-promotion
        -Wfloat-equal
        # Correctness style rules.
        -Wshadow
        -Wnon-virtual-dtor
        -Woverloaded-virtual
        -Wold-style-cast
        -Wcast-align
        -Wnull-dereference
        -Wvla
        -Warray-bounds=2
        -Wstringop-overflow=4
        # printf-style format and diagnostics.
        -Wformat=2
        -Wimplicit-fallthrough=5
        # Hardening.
        -fstack-protector-strong
        -fno-common
    )
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # GCC-only checks, Clang rejects some of them.
        list(APPEND TONE_PLAYER_WARNING_FLAGS
            -Wlogical-op
            -Wduplicated-cond
            -Wduplicated-branches
            -Wuseless-cast
        )
    endif()
elseif(MSVC)
    # WinMain entry: keep WIN32_EXECUTABLE effective and avoid setlocale issues
    # with UTF-8 sources and resources.
    set(TONE_PLAYER_WARNING_FLAGS
        /W4
        /permissive-
        /Zc:__cplusplus
        /Zc:preprocessor
        /utf-8
        # Signed/unsigned and narrowing equivalents.
        /w14242
        /w14254
        /w14263
        /w14265
        /w14287
        /w14296
        /w14311
        # Codegen parity for common suspects.
        /w14640
        /w14826
        /w14905
        /w14906
        /w14928
    )
    # Close to the GCC hardening set above.
    target_compile_options(tone-player-qt PRIVATE /sdl) # extra lifetime/undef checks
endif()
if(TONE_PLAYER_WARNINGS_AS_ERRORS)
    if(MSVC)
        list(APPEND TONE_PLAYER_WARNING_FLAGS /WX)
    else()
        list(APPEND TONE_PLAYER_WARNING_FLAGS -Werror)
    endif()
endif()
if(TONE_PLAYER_WARNING_FLAGS)
    target_compile_options(tone-player-qt PRIVATE ${TONE_PLAYER_WARNING_FLAGS})
endif()

# Build machine tuning, opt-in: the produced binary stops being portable.
# MSVC equivalent (/arch:) is intentionally not enabled by default.
if(TONE_PLAYER_NATIVE_OPT AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(tone-player-qt PRIVATE -march=native)
endif()

# Sanitizers for the development builds on Linux (mutually exclusive with LTO
# and with the optimized release flags).
if(TONE_PLAYER_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(STATUS "Sanitizers enabled: ASan + UBSan (development build)")
    target_compile_options(tone-player-qt PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
        $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
        $<$<CONFIG:Debug>:-fno-sanitize=signed-integer-overflow>)
    target_link_options(tone-player-qt PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
endif()

# Release default of -O2 is fine for the audio engine; LTO buys a bit more.
# Qt's third-party templates sometimes warn, so warnings apply only to our
# sources (compile options on the target, not INTERFACE).
if(CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo|MinSizeRel")
    check_ipo_supported(RESULT TONE_PLAYER_IPO OUTPUT TONE_PLAYER_IPO_MSG)
    if(TONE_PLAYER_LTO AND TONE_PLAYER_IPO)
        set_target_properties(tone-player-qt PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION TRUE)
    elseif(TONE_PLAYER_LTO)
        message(STATUS "LTO not enabled (${TONE_PLAYER_IPO_MSG})")
    endif()
endif()

