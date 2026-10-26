// Reading, validation and writing of the settings.json file.
//
// There are no default values: the file must exist and contain all parameters.
// Any deviation (syntax, type, range, unknown key) is considered corruption.
#pragma once

#include "models.h"

#include <QJsonObject>
#include <QString>
#include <filesystem>
#include <string_view>
#include <optional>

namespace tp::settings_store {

inline constexpr std::string_view settingsName = "settings.json";
inline constexpr int schemaVersion = 1;
// Token of the default device in the file: device and active preset.
inline constexpr std::string_view defaultDeviceToken = "default";

// Why the preset name is invalid, or std::nullopt if it is valid.
[[nodiscard]] std::optional<QString> presetNameProblem(const QString &name);
void validateLevelScale(const LevelScale &scale);
void validateBands(const QVector<Band> &bands, double maxFrequencyHz, const QString &what,
    const LevelScale &scale = defaultLevelScale);
void validateSettings(const Settings &settings);

// Build and validate the settings from parsed json.
[[nodiscard]] Settings parseSettings(const QJsonObject &root);
// Read settings.json. Without the file the program cannot work.
[[nodiscard]] Settings loadSettings(const std::filesystem::path &path);
// Load at program start: the corrupted file is moved aside.
[[nodiscard]] Settings loadSettingsAtStartup(const std::filesystem::path &path);

// Text of settings.json to write to disk.
[[nodiscard]] QString settingsToText(const Settings &settings);
// Write settings.json atomically: temporary file nearby, then replace.
void saveSettings(const std::filesystem::path &path, const Settings &settings);
// Rename the corrupted file into settings.corrupted-<date-time>.json.
[[nodiscard]] std::filesystem::path quarantineCorrupted(const std::filesystem::path &path);

// Short number representation in the style of the original encoder: "0.5",
// "500.0", "-12.0". Exported for tests.
[[nodiscard]] QString scalarText(double value);

} // namespace tp::settings_store
