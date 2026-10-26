// Preset operations: pure functions over Settings, without Qt and files.
#pragma once

#include "models.h"

#include <QString>
#include <optional>

namespace tp::presets {

// Result of applying a preset.
struct PresetApplication {
    Settings settings;
    // Device from the preset that does not exist on this computer; the
    // default output device is used instead.
    std::optional<QString> missingDevice;
};

// Name without extra spaces at the edges; throws PresetError if it is invalid.
QString normalizeName(const QString &name);
// Preset with the same name; case-insensitive.
std::optional<Preset> findPreset(const Settings &settings, const QString &name);
// Preset levels filled to the global list size; new bands get 0 dB.
QVector<double> alignedLevels(const QVector<double> &levels, int count);

// Save levels and the device under the name and make the preset active.
// Frequencies are not part of a preset: the list is shared across the program.
// An existing preset is replaced only with overwrite; it stays in place.
Settings savePreset(const Settings &settings, const QString &name,
    const QVector<double> &levels, bool overwrite);
// Delete the preset; the settings stay as they are, the active one resets.
Settings deletePreset(const Settings &settings, const QString &name);
// Rename the preset; the name must not collide with another preset.
Settings renamePreset(const Settings &settings, const QString &name, const QString &newName);
// Apply preset levels to the shared list of frequencies and select its device.
PresetApplication applyPreset(const Settings &settings, const QString &name,
    const QSet<QString> &availableDevices);

// Do the current levels or device differ from the active preset?
bool isModified(const Settings &settings, const QVector<double> &currentLevels = {});

} // namespace tp::presets
