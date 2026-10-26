/// @file text_format.h
/// @brief Formatting numbers for the interface.
#pragma once

#include <QString>

/// @brief Presentation of frequencies, durations and levels.
namespace text_format {

/// @brief Returns a frequency value as a number with a space as the group separator, no unit.
/// @param frequency Frequency, Hz.
QString formatFrequencyValue(double frequency);

/// @brief Returns a human-readable time: minutes and seconds, rounded up.
/// @param seconds Duration, seconds.
QString formatDuration(double seconds);

/// @brief Returns a level in dB; instead of a hyphen a real minus, extra zeros are removed.
/// @param value Level, dB.
/// @param unit Whether to attach the unit from the translation.
QString formatDb(double value, bool unit = true);

/// @brief Returns a label of a scale division of the level: “−10”, “0”, “+7.5”.
/// @param valueDb Division value, dB.
QString formatScale(double valueDb);

/// @brief Returns a short representation of a double in the style of the json encoder of
/// the original: "0.5", "500.0", "-12.0" (exported for the settings store).
/// @param value Number to format.
QString pythonFloat(double value);

/// @brief Returns the shortest general-format representation of a double: "7.5", "10"
/// (exported for error texts with numbers).
/// @param value Number to format.
QString generalNumber(double value);

} // namespace text_format
