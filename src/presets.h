/// @file presets.h
/// @brief Preset operations: pure functions over Settings, without Qt and files.
#pragma once

#include "models.h"

#include <QString>
#include <optional>

/// @brief Creating, applying and comparing presets.
namespace tp::presets {

/// @brief Result of applying a preset.
struct PresetApplication {
    Settings settings; ///< Settings with the preset levels and device applied.
    /// Device from the preset that does not exist on this computer; the
    /// default output device is used instead.
    std::optional<QString> missingDevice;
};

/// @brief Returns the name without extra spaces at the edges.
/// @param name Raw preset name.
/// @return Trimmed name ready for storage.
/// @throws PresetError If the name is empty or too long.
QString normalizeName(const QString &name);

/// @brief Finds a preset with the same name; case-insensitive.
/// @param settings Settings whose preset list is searched.
/// @param name Preset name to find.
/// @return The preset, or std::nullopt if it does not exist.
std::optional<Preset> findPreset(const Settings &settings, const QString &name);

/// @brief Returns preset levels filled to the global band list size; new bands get 0 dB.
/// @param levels Stored preset levels.
/// @param count Required number of levels.
QVector<double> alignedLevels(const QVector<double> &levels, int count);

/// @brief Saves levels and the device under the name and makes the preset active.
///
/// Frequencies are not part of a preset: the list is shared across the program.
/// An existing preset is replaced only with @p overwrite; it stays in place.
/// @param settings Current settings.
/// @param name Preset name.
/// @param levels Band level offsets to store, dB.
/// @param overwrite Whether an existing preset with the same name may be replaced.
/// @return Settings with the saved and active preset.
Settings savePreset(const Settings &settings, const QString &name,
    const QVector<double> &levels, bool overwrite);

/// @brief Deletes the preset; the settings stay as they are, the active one resets.
/// @param settings Current settings.
/// @param name Preset name.
/// @return Settings without the preset.
Settings deletePreset(const Settings &settings, const QString &name);

/// @brief Renames the preset; the name must not collide with another preset.
/// @param settings Current settings.
/// @param name Current preset name.
/// @param newName New preset name.
/// @return Settings with the renamed preset.
Settings renamePreset(const Settings &settings, const QString &name, const QString &newName);

/// @brief Applies preset levels to the shared list of frequencies and selects its device.
/// @param settings Current settings.
/// @param name Preset name.
/// @param availableDevices Labels of the devices present on this computer.
/// @return Applied settings and an optional missing-device warning.
PresetApplication applyPreset(const Settings &settings, const QString &name,
    const QSet<QString> &availableDevices);

/// @brief Tells whether the current levels or device differ from the active preset.
/// @param settings Current settings.
/// @param currentLevels Levels to compare; empty means the levels stored in @p settings.
bool isModified(const Settings &settings, const QVector<double> &currentLevels = {});

} // namespace tp::presets
