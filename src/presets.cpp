/// @file presets.cpp
/// @brief Preset operations: pure functions over Settings, without Qt and files.
///
/// Implementation of the preset operations declared in presets.h.
#include "presets.h"

#include "errors.h"
#include "i18n.h"
#include "settings_store.h"

#include <QSet>
#include <QtGlobal>

namespace tp::presets {

using ::PresetError;

namespace {

using tr_ns::tr;

} // namespace

QString normalizeName(const QString &name)
{
    const QString stripped = name.trimmed();
    const auto problem = settings_store::presetNameProblem(stripped);
    if (problem.has_value()) {
        throw PresetError(problem.value());
    }
    return stripped;
}

std::optional<Preset> findPreset(const Settings &settings, const QString &name)
{
    const QString key = name.toCaseFolded();
    for (const Preset &preset : settings.presets) {
        if (preset.name.toCaseFolded() == key) {
            return preset;
        }
    }
    return std::nullopt;
}

QVector<double> alignedLevels(const QVector<double> &levels, const int count)
{
    QVector<double> result;
    result.reserve(count);
    for (int index = 0; index < count; ++index) {
        result.push_back(index < levels.size() ? levels.at(index) : 0.0);
    }
    return result;
}

Settings savePreset(const Settings &settings, const QString &name,
    const QVector<double> &levels, const bool overwrite)
{
    QString normalized;
    try {
        normalized = normalizeName(name);
    } catch (const PresetError &error) {
        throw PresetError(error.message());
    }
    const auto existing = findPreset(settings, normalized);
    if (existing.has_value() && !overwrite) {
        throw PresetError(tr("preset.alreadyExists", existing->name));
    }

    const QVector<double> presetLevels = levels;
    const Preset preset{normalized, settings.device, presetLevels};

    Settings result = settings;
    if (!existing.has_value()) {
        result.presets.push_back(preset);
    } else {
        for (Preset &item : result.presets) {
            if (item == existing) {
                item = preset;
            }
        }
    }
    result.activePreset = normalized;
    return result;
}

Settings deletePreset(const Settings &settings, const QString &name)
{
    const Preset *target = nullptr;
    const QString key = name.toCaseFolded();
    for (const Preset &preset : settings.presets) {
        if (preset.name.toCaseFolded() == key) {
            target = &preset;
            break;
        }
    }
    if (target == nullptr) {
        throw PresetError(tr("preset.notFound", name));
    }

    Settings result = settings;
    const bool wasActive = settings.activePreset.has_value()
        && (settings.activePreset->toCaseFolded() == key);
    result.presets.removeIf([&target](const Preset &item) { return item == *target; });
    result.activePreset = wasActive ? std::nullopt : settings.activePreset;
    return result;
}

Settings renamePreset(const Settings &settings, const QString &name, const QString &newName)
{
    const auto target = findPreset(settings, name);
    if (!target.has_value()) {
        throw PresetError(tr("preset.notFound", name));
    }
    QString normalized;
    try {
        normalized = normalizeName(newName);
    } catch (const PresetError &error) {
        throw PresetError(error.message());
    }
    const auto other = findPreset(settings, normalized);
    if (other.has_value() && !(other == target)) {
        throw PresetError(tr("preset.alreadyExists", other->name));
    }

    Settings result = settings;
    Preset renamed = *target;
    renamed.name = normalized;
    for (Preset &item : result.presets) {
        if (item == *target) {
            item = renamed;
        }
    }
    const bool isActive = settings.activePreset.has_value()
        && (settings.activePreset->toCaseFolded() == target->name.toCaseFolded());
    result.activePreset = isActive ? normalized : settings.activePreset;
    return result;
}

PresetApplication applyPreset(const Settings &settings, const QString &name,
    const QSet<QString> &availableDevices)
{
    const auto preset = findPreset(settings, name);
    if (!preset.has_value()) {
        throw PresetError(tr("preset.notFound", name));
    }
    const QVector<double> levels = alignedLevels(preset->levels, static_cast<int>(settings.bands.size()));
    Q_ASSERT(settings.bands.size() == levels.size());
    QVector<Band> bands;
    bands.reserve(settings.bands.size());
    for (int index = 0; index < settings.bands.size(); ++index) {
        bands.push_back(Band{settings.bands.at(index).frequencyHz, levels.at(index)});
    }

    auto device = preset->device;
    std::optional<QString> missing;
    if (device.has_value() && !availableDevices.contains(device.value())) {
        missing = device;
        device = std::nullopt;
    }

    return PresetApplication{
        .settings = [&] {
            Settings result = settings;
            result.bands = bands;
            result.device = device;
            result.activePreset = preset->name;
            return result;
        }(),
        .missingDevice = missing,
    };
}

bool isModified(const Settings &settings, const QVector<double> &currentLevels)
{
    if (!settings.activePreset.has_value()) {
        return false;
    }
    const auto preset = findPreset(settings, settings.activePreset.value());
    if (!preset.has_value()) {
        return false;
    }
    const QVector<double> expected =
        alignedLevels(preset->levels, static_cast<int>(settings.bands.size()));
    const QVector<double> levels =
        currentLevels.isEmpty() ? settings.levels() : currentLevels;
    return levels != expected || settings.device != preset->device;
}

} // namespace tp::presets
