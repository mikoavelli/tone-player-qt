// Settings models and playback state.
#pragma once

#include <QString>
#include <QVector>
#include <qglobal.h>
#include <cmath>
#include <optional>
#include <tuple>

namespace tp {

// -----------------------------------------------------------------------------
// Allowed setting values. Nowhere else, except this file, numbers are not
// duplicated: the bounds are used by file validation, the engine, and the window.
// -----------------------------------------------------------------------------

// Offset of the equalizer band level relative to the overall level, dB.
// These are default values: the range and step of the sliders are stored in
// settings.
inline constexpr double defaultLevelStepDb = 0.5;
inline constexpr double defaultLevelMinDb = -10.0;
inline constexpr double defaultLevelMaxDb = 10.0;

// Previous names remain for code that needs exactly the default range.
inline constexpr double levelStepDb = defaultLevelStepDb;
inline constexpr double levelMinDb = defaultLevelMinDb;
inline constexpr double levelMaxDb = defaultLevelMaxDb;

// Allowed bounds of the range and step of the level sliders.
inline constexpr double minLevelScaleDb = -30.0;
inline constexpr double maxLevelScaleDb = 30.0;
inline constexpr double minLevelStepDb = 0.1;
inline constexpr double maxLevelStepDb = 5.0;

// Overall level relative to full scale, dB (0 dB = amplitude 1.0).
inline constexpr double masterMinDb = -60.0;
inline constexpr double masterMaxDb = 0.0;
// Base level: the master slider shows the offset from it.
inline constexpr double masterBaseDb = -12.0;

inline constexpr double minFrequencyHz = 20.0;
inline constexpr int maxBands = 64;

inline constexpr double minSecondsPerTone = 1.0;
inline constexpr double maxSecondsPerTone = 3600.0;

inline constexpr int minSampleRate = 8000;
inline constexpr int maxSampleRate = 768000;

inline constexpr double minFadeMs = 1.0;
inline constexpr double maxFadeMs = 1000.0;

inline constexpr int maxPresetNameLength = 64;

inline constexpr double stepTolerance = 1e-9;

// Range and step of the level sliders, dB.
//
// The sliders of all bands and of the master use one scale, therefore a slider
// position is a step number from `minDb` to `maxDb` with step `stepDb`.
class LevelScale {
public:
    constexpr LevelScale(
        double minValue = defaultLevelMinDb,
        double maxValue = defaultLevelMaxDb,
        double stepValue = defaultLevelStepDb)
        : minDb(minValue)
        , maxDb(maxValue)
        , stepDb(stepValue)
    {
    }

    [[nodiscard]] bool operator==(const LevelScale &other) const = default;

    // Range width, dB.
    [[nodiscard]] constexpr double spanDb() const { return maxDb - minDb; }

    // Number of steps between the extreme slider positions.
    [[nodiscard]] int positionCount() const
    {
        const int steps = static_cast<int>(std::lround(spanDb() / stepDb));
        return std::max(steps, 1);
    }

    // Does the value lie on the step grid.
    [[nodiscard]] bool onStep(double valueDb) const
    {
        const double steps = valueDb / stepDb;
        return std::abs(steps - std::round(steps)) <= stepTolerance;
    }

    // Nearest value on the step grid.
    [[nodiscard]] double snap(double valueDb) const
    {
        return roundTo6(std::round(valueDb / stepDb) * stepDb);
    }

    // Value within the range, on the step grid.
    [[nodiscard]] double clamp(double valueDb) const
    {
        return std::clamp(snap(valueDb), minDb, maxDb);
    }

    // Slider position for a value, in steps from the minimum.
    [[nodiscard]] int position(double valueDb) const
    {
        const auto value = std::lround((valueDb - minDb) / stepDb);
        return static_cast<int>(std::clamp<qint64>(value, 0, positionCount()));
    }

    // Value on the grid for a slider position.
    [[nodiscard]] double atPosition(int position) const
    {
        return roundTo6(minDb + position * stepDb);
    }

    double minDb;
    double maxDb;
    double stepDb;

private:
    static double roundTo6(double value)
    {
        return std::round(value * 1e6) / 1e6;
    }
};

inline constexpr LevelScale defaultLevelScale{defaultLevelMinDb, defaultLevelMaxDb,
    defaultLevelStepDb};
// The master slider always moves ±10 dB, independent of the band range.
inline constexpr LevelScale masterGainScale{defaultLevelMinDb, defaultLevelMaxDb,
    defaultLevelStepDb};

// Nyquist frequency: a tone at it or above would be distorted by aliasing.
[[nodiscard]] inline double nyquistHz(int sampleRate) { return sampleRate / 2.0; }

// Player state.
enum class PlayerState {
    Idle,
    Playing,
    Paused,
    Finished,
};

// Equalizer band: the tone frequency and the offset of its level.
struct Band {
    double frequencyHz;
    double levelDb;

    [[nodiscard]] bool operator==(const Band &other) const = default;
};

// Named set of levels for the shared list of frequencies and the output device.
struct Preset {
    QString name;
    std::optional<QString> device;
    QVector<double> levels;

    [[nodiscard]] bool operator==(const Preset &other) const = default;
};

// All program settings; stored in settings.json next to the program.
struct Settings {
    double secondsPerTone = 0.0;
    int sampleRate = 0;
    double fadeMs = 0.0;
    double masterGainDb = 0.0;
    std::optional<QString> device;
    QVector<Band> bands;
    QVector<Preset> presets;
    std::optional<QString> activePreset;
    LevelScale levelScale;
    bool autoplay = true;

    // Band frequencies in ascending order, Hz.
    [[nodiscard]] QVector<double> frequencies() const
    {
        QVector<double> result;
        result.reserve(bands.size());
        for (const Band &band : bands) {
            result.push_back(band.frequencyHz);
        }
        return result;
    }

    // Band level offsets, dB, in frequency order.
    [[nodiscard]] QVector<double> levels() const
    {
        QVector<double> result;
        result.reserve(bands.size());
        for (const Band &band : bands) {
            result.push_back(band.levelDb);
        }
        return result;
    }

    [[nodiscard]] bool operator==(const Settings &other) const = default;
};

// Structure of a playback session: what and how long to play.
//
// Levels are not part of it: they change during sounding and live in the
// generator.
struct ToneConfig {
    QVector<double> frequencies;
    double secondsPerTone = 0.0;
    int sampleRate = 0;
    double fadeMs = 0.0;

    [[nodiscard]] static ToneConfig fromSettings(const Settings &settings)
    {
        return {settings.frequencies(), settings.secondsPerTone, settings.sampleRate,
            settings.fadeMs};
    }

    // Number of frequencies.
    [[nodiscard]] int toneCount() const { return static_cast<int>(frequencies.size()); }

    // Total duration of the session, seconds.
    [[nodiscard]] double totalSeconds() const
    {
        return secondsPerTone * static_cast<double>(frequencies.size());
    }

    // Duration of a tone ramp up/down, seconds.
    [[nodiscard]] double fadeSeconds() const
    {
        return std::min(fadeMs / 1000.0, secondsPerTone / 2.0);
    }

    // Tone frequency by index, Hz.
    [[nodiscard]] double frequencyAt(int index) const { return frequencies.at(index); }
};

// Current playback position.
struct PlaybackProgress {
    int toneIndex = 0;
    double toneElapsed = 0.0;
    double toneRemaining = 0.0;
    double totalElapsed = 0.0;
    double totalRemaining = 0.0;

    // Fraction of time passed of the current tone, 0.0..1.0.
    [[nodiscard]] double toneFraction() const
    {
        const double total = toneElapsed + toneRemaining;
        if (total <= 0.0 || !std::isfinite(total)) {
            return 0.0;
        }
        return std::clamp(toneElapsed / total, 0.0, 1.0);
    }

    // Fraction of time passed of the session, 0.0..1.0.
    [[nodiscard]] double totalFraction() const
    {
        const double total = totalElapsed + totalRemaining;
        if (total <= 0.0 || !std::isfinite(total)) {
            return 0.0;
        }
        return std::clamp(totalElapsed / total, 0.0, 1.0);
    }
};

} // namespace tp
