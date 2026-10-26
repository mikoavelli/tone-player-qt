/// @file settings_store.h
/// @brief Reading, validation and writing of the settings.json file.
///
/// There are no default values: the file must exist and contain all parameters.
/// Any deviation (syntax, type, range, unknown key) is considered corruption.
#pragma once

#include "models.h"

#include <QJsonObject>
#include <QString>
#include <filesystem>
#include <string_view>
#include <optional>

/// @brief Persistence and validation of the program settings.
namespace tp::settings_store {

/// @brief File name of the settings next to the program.
inline constexpr std::string_view settingsName = "settings.json";
/// @brief Schema version stored in the file.
inline constexpr int schemaVersion = 1;
/// @brief Token of the default device in the file: device and active preset.
inline constexpr std::string_view defaultDeviceToken = "default";

/// @brief Explains why the preset name is invalid.
/// @param name Preset name to check.
/// @return Problem description, or std::nullopt if the name is valid.
[[nodiscard]] std::optional<QString> presetNameProblem(const QString &name);

/// @brief Validates the range and step of the level sliders.
/// @param scale Scale to check.
/// @throws SettingsFormatError If the scale is outside the allowed bounds.
void validateLevelScale(const LevelScale &scale);

/// @brief Validates the band list: count, order, frequencies and levels.
/// @param bands Bands to check.
/// @param maxFrequencyHz Highest allowed frequency (Nyquist), Hz.
/// @param what Human-readable name of the checked data for error messages.
/// @param scale Range and step of the level offsets.
/// @throws SettingsFormatError If the bands are invalid.
void validateBands(const QVector<Band> &bands, double maxFrequencyHz, const QString &what,
    const LevelScale &scale = defaultLevelScale);

/// @brief Validates all fields of the settings object.
/// @param settings Settings to check.
/// @throws SettingsFormatError If any value is outside the allowed range.
void validateSettings(const Settings &settings);

/// @brief Builds and validates the settings from parsed json.
/// @param root Parsed content of settings.json.
/// @return Validated settings.
[[nodiscard]] Settings parseSettings(const QJsonObject &root);

/// @brief Reads settings.json. Without the file the program cannot work.
/// @param path Path of the file.
/// @return Validated settings.
/// @throws SettingsNotFoundError If the file does not exist.
/// @throws SettingsReadError If the file cannot be read.
/// @throws SettingsFormatError If the content is invalid.
[[nodiscard]] Settings loadSettings(const std::filesystem::path &path);

/// @brief Loads at program start: the corrupted file is moved aside.
/// @param path Path of the file.
/// @return Validated settings.
[[nodiscard]] Settings loadSettingsAtStartup(const std::filesystem::path &path);

/// @brief Returns the text of settings.json to write to disk.
/// @param settings Settings to serialize.
[[nodiscard]] QString settingsToText(const Settings &settings);

/// @brief Writes settings.json atomically: temporary file nearby, then replace.
/// @param path Path of the file.
/// @param settings Settings to write.
/// @throws SettingsSaveError If the file cannot be written.
void saveSettings(const std::filesystem::path &path, const Settings &settings);

/// @brief Renames the corrupted file into settings.corrupted-&lt;date-time&gt;.json.
/// @param path Path of the corrupted file.
/// @return Path of the moved file.
[[nodiscard]] std::filesystem::path quarantineCorrupted(const std::filesystem::path &path);

/// @brief Short number representation in the style of the original encoder: "0.5",
/// "500.0", "-12.0". Exported for tests.
/// @param value Number to format.
[[nodiscard]] QString scalarText(double value);

} // namespace tp::settings_store
