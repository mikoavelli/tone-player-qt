// Setting numbers for the interface.
#pragma once

#include <QString>

namespace text_format {

// Frequency value as a number with a space as the group separator, no unit.
QString formatFrequencyValue(double frequency);

// Human-readable time: minutes and seconds, rounded up.
QString formatDuration(double seconds);

// Level in dB; instead of a hyphen a real minus, extra zeros are removed.
// With an argument, the unit from the translation is attached.
QString formatDb(double value, bool unit = true);

// Label of a scale division of the level: “−10”, “0”, “+7.5”.
QString formatScale(double valueDb);

// Short representation of a double in the style of the json encoder of the
// original: "0.5", "500.0", "-12.0" (exported for the settings store).
QString pythonFloat(double value);

// Shortest general-format representation of a double: "7.5", "10" (exported
// for error texts with numbers).
QString generalNumber(double value);

} // namespace text_format
