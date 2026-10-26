/// @file models.h
/// @brief Settings models and playback state.
///
/// Allowed setting values live here and are not duplicated anywhere else: the
/// settings validator, the audio engine and the window all use these constants.
#pragma once

#include <QString>
#include <QVector>
#include <qglobal.h>
#include <cmath>
#include <optional>
#include <tuple>

/// @brief Core domain models and constants shared by all parts of the program.
namespace tp {

/// @name Allowed setting values
/// @{

/// @brief Default step of an equalizer band level, dB.
inline constexpr double defaultLevelStepDb = 0.5;
/// @brief Default minimum of an equalizer band level, dB.
inline constexpr double defaultLevelMinDb = -10.0;
/// @brief Default maximum of an equalizer band level, dB.
inline constexpr double defaultLevelMaxDb = 10.0;

/// @brief Alias of the default level step for code that needs exactly the default range.
inline constexpr double levelStepDb = defaultLevelStepDb;
/// @brief Alias of the default minimum for code that needs exactly the default range.
inline constexpr double levelMinDb = defaultLevelMinDb;
/// @brief Alias of the default maximum for code that needs exactly the default range.
inline constexpr double levelMaxDb = defaultLevelMaxDb;

/// @brief Lowest allowed minimum of the level slider range, dB.
inline constexpr double minLevelScaleDb = -30.0;
/// @brief Highest allowed maximum of the level slider range, dB.
inline constexpr double maxLevelScaleDb = 30.0;
/// @brief Smallest allowed level slider step, dB.
inline constexpr double minLevelStepDb = 0.1;
/// @brief Largest allowed level slider step, dB.
inline constexpr double maxLevelStepDb = 5.0;

/// @brief Lowest overall level relative to full scale, dB (0 dB = amplitude 1.0).
inline constexpr double masterMinDb = -60.0;
/// @brief Highest overall level relative to full scale, dB.
inline constexpr double masterMaxDb = 0.0;
/// @brief Base level: the master slider shows the offset from it, dB.
inline constexpr double masterBaseDb = -12.0;

/// @brief Lowest allowed tone frequency, Hz.
inline constexpr double minFrequencyHz = 20.0;
/// @brief Maximum number of equalizer bands.
inline constexpr int maxBands = 64;

/// @brief Shortest allowed tone duration, seconds.
inline constexpr double minSecondsPerTone = 1.0;
/// @brief Longest allowed tone duration, seconds.
inline constexpr double maxSecondsPerTone = 3600.0;

/// @brief Lowest allowed sample rate, Hz.
inline constexpr int minSampleRate = 8000;
/// @brief Highest allowed sample rate, Hz.
inline constexpr int maxSampleRate = 768000;

/// @brief Shortest allowed fade, ms.
inline constexpr double minFadeMs = 1.0;
/// @brief Longest allowed fade, ms.
inline constexpr double maxFadeMs = 1000.0;

/// @brief Maximum length of a preset name, characters.
inline constexpr int maxPresetNameLength = 64;

/// @brief Tolerance when checking that a value lies on the step grid.
inline constexpr double stepTolerance = 1e-9;

/// @}

/// @brief Range and step of the level sliders, dB.
///
/// The sliders of all bands and of the master use one scale, therefore a slider
/// position is a step number from @ref minDb to @ref maxDb with step @ref stepDb.
class LevelScale {
public:
    /// @brief Creates a scale from a range and a step.
    /// @param minValue Minimum slider value, dB.
    /// @param maxValue Maximum slider value, dB.
    /// @param stepValue Slider step, dB.
    constexpr LevelScale(
        double minValue = defaultLevelMinDb,
        double maxValue = defaultLevelMaxDb,
        double stepValue = defaultLevelStepDb)
        : minDb(minValue)
        , maxDb(maxValue)
        , stepDb(stepValue)
    {
    }

    /// @brief Compares the range and the step of two scales.
    [[nodiscard]] bool operator==(const LevelScale &other) const = default;

    /// @brief Range width, dB.
    [[nodiscard]] constexpr double spanDb() const { return maxDb - minDb; }

    /// @brief Number of steps between the extreme slider positions.
    [[nodiscard]] int positionCount() const
    {
        const int steps = static_cast<int>(std::lround(spanDb() / stepDb));
        return std::max(steps, 1);
    }

    /// @brief Checks whether a value lies on the step grid.
    /// @param valueDb Value to check, dB.
    /// @return @c true if the value is within @ref stepTolerance of a grid point.
    [[nodiscard]] bool onStep(double valueDb) const
    {
        const double steps = valueDb / stepDb;
        return std::abs(steps - std::round(steps)) <= stepTolerance;
    }

    /// @brief Returns the nearest value on the step grid.
    /// @param valueDb Value to snap, dB.
    [[nodiscard]] double snap(double valueDb) const
    {
        return roundTo6(std::round(valueDb / stepDb) * stepDb);
    }

    /// @brief Returns a value within the range, on the step grid.
    /// @param valueDb Value to clamp, dB.
    [[nodiscard]] double clamp(double valueDb) const
    {
        return std::clamp(snap(valueDb), minDb, maxDb);
    }

    /// @brief Returns the slider position for a value, in steps from the minimum.
    /// @param valueDb Value to convert, dB.
    [[nodiscard]] int position(double valueDb) const
    {
        const auto value = std::lround((valueDb - minDb) / stepDb);
        return static_cast<int>(std::clamp<qint64>(value, 0, positionCount()));
    }

    /// @brief Returns the value on the grid for a slider position.
    /// @param position Slider position in steps from the minimum.
    [[nodiscard]] double atPosition(int position) const
    {
        return roundTo6(minDb + position * stepDb);
    }

    double minDb; ///< Minimum slider value, dB.
    double maxDb; ///< Maximum slider value, dB.
    double stepDb; ///< Slider step, dB.

private:
    /// @brief Rounds a value to six decimal places to hide binary noise.
    static double roundTo6(double value)
    {
        return std::round(value * 1e6) / 1e6;
    }
};

/// @brief Scale of the equalizer band sliders.
inline constexpr LevelScale defaultLevelScale{defaultLevelMinDb, defaultLevelMaxDb,
    defaultLevelStepDb};
/// @brief Scale of the master slider: always ±10 dB, independent of the band range.
inline constexpr LevelScale masterGainScale{defaultLevelMinDb, defaultLevelMaxDb,
    defaultLevelStepDb};

/// @brief Returns the Nyquist frequency for a sample rate, Hz.
///
/// A tone at it or above would be distorted by aliasing.
[[nodiscard]] inline double nyquistHz(int sampleRate) { return sampleRate / 2.0; }

/// @brief State of the player.
enum class PlayerState {
    Idle, ///< No session has been started.
    Playing, ///< Sound is being generated.
    Paused, ///< The session is suspended and can be resumed.
    Finished, ///< The last tone has been played completely.
};

/// @brief Equalizer band: the tone frequency and the offset of its level.
struct Band {
    double frequencyHz; ///< Tone frequency, Hz.
    double levelDb; ///< Level offset relative to the overall level, dB.

    /// @brief Compares the frequency and the level of two bands.
    [[nodiscard]] bool operator==(const Band &other) const = default;
};

/// @brief Named set of levels for the shared list of frequencies and the output device.
struct Preset {
    QString name; ///< Preset name as entered; comparisons are case-insensitive.
    std::optional<QString> device; ///< Output device label; empty means the default device.
    QVector<double> levels; ///< Band level offsets, dB.

    /// @brief Compares all fields of two presets.
    [[nodiscard]] bool operator==(const Preset &other) const = default;
};

/// @brief All program settings; stored in settings.json next to the program.
struct Settings {
    double secondsPerTone = 0.0; ///< Tone duration, seconds.
    int sampleRate = 0; ///< Sample rate, Hz.
    double fadeMs = 0.0; ///< Fade-in and fade-out duration, ms.
    double masterGainDb = 0.0; ///< Overall level, dB from full scale.
    std::optional<QString> device; ///< Output device label; empty means the default device.
    QVector<Band> bands; ///< Equalizer bands in ascending frequency order.
    QVector<Preset> presets; ///< Saved presets.
    std::optional<QString> activePreset; ///< Name of the active preset, if any.
    LevelScale levelScale; ///< Range and step of the level sliders, dB.
    bool autoplay = true; ///< Continue with the next tone automatically.

    /// @brief Returns the band frequencies in ascending order, Hz.
    [[nodiscard]] QVector<double> frequencies() const
    {
        QVector<double> result;
        result.reserve(bands.size());
        for (const Band &band : bands) {
            result.push_back(band.frequencyHz);
        }
        return result;
    }

    /// @brief Returns the band level offsets in frequency order, dB.
    [[nodiscard]] QVector<double> levels() const
    {
        QVector<double> result;
        result.reserve(bands.size());
        for (const Band &band : bands) {
            result.push_back(band.levelDb);
        }
        return result;
    }

    /// @brief Compares all fields of two settings objects.
    [[nodiscard]] bool operator==(const Settings &other) const = default;
};

/// @brief Structure of a playback session: what and how long to play.
///
/// Levels are not part of it: they change during sounding and live in the
/// generator.
struct ToneConfig {
    QVector<double> frequencies; ///< Tone frequencies in playback order, Hz.
    double secondsPerTone = 0.0; ///< Duration of one tone, seconds.
    int sampleRate = 0; ///< Sample rate, Hz.
    double fadeMs = 0.0; ///< Fade-in and fade-out duration, ms.

    /// @brief Builds the playback configuration from the program settings.
    /// @param settings Settings to take the frequencies, duration, rate and fade from.
    [[nodiscard]] static ToneConfig fromSettings(const Settings &settings)
    {
        return {settings.frequencies(), settings.secondsPerTone, settings.sampleRate,
            settings.fadeMs};
    }

    /// @brief Returns the number of frequencies.
    [[nodiscard]] int toneCount() const { return static_cast<int>(frequencies.size()); }

    /// @brief Returns the total duration of the session, seconds.
    [[nodiscard]] double totalSeconds() const
    {
        return secondsPerTone * static_cast<double>(frequencies.size());
    }

    /// @brief Returns the duration of one ramp up or down, seconds.
    [[nodiscard]] double fadeSeconds() const
    {
        return std::min(fadeMs / 1000.0, secondsPerTone / 2.0);
    }

    /// @brief Returns the tone frequency by index, Hz.
    /// @param index Index in @ref frequencies.
    [[nodiscard]] double frequencyAt(int index) const { return frequencies.at(index); }
};

/// @brief Current playback position.
struct PlaybackProgress {
    int toneIndex = 0; ///< Index of the current (or selected) tone.
    double toneElapsed = 0.0; ///< Time played of the current tone, seconds.
    double toneRemaining = 0.0; ///< Time left of the current tone, seconds.
    double totalElapsed = 0.0; ///< Time played of the whole session, seconds.
    double totalRemaining = 0.0; ///< Time left of the whole session, seconds.

    /// @brief Returns the fraction of time passed of the current tone, 0.0..1.0.
    [[nodiscard]] double toneFraction() const
    {
        const double total = toneElapsed + toneRemaining;
        if (total <= 0.0 || !std::isfinite(total)) {
            return 0.0;
        }
        return std::clamp(toneElapsed / total, 0.0, 1.0);
    }

    /// @brief Returns the fraction of time passed of the session, 0.0..1.0.
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
