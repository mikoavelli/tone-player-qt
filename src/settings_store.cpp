/// @file settings_store.cpp
/// @brief Reading, validation and writing of the settings.json file.
///
/// Implementation of the settings persistence declared in settings_store.h.
#include "settings_store.h"

#include "errors.h"
#include "exact_eq.h"
#include "i18n.h"
#include "theme.h"
#include "text_format.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSaveFile>
#include <QSet>
#include <QStringDecoder>
#include <QStringList>
#include <QTemporaryDir>
#include <QtGlobal>
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>

namespace tp::settings_store {

using ::SettingsFormatError;
using ::SettingsNotFoundError;
using ::SettingsReadError;
using ::SettingsSaveError;
using ::SettingsVersionError;

namespace {

using tr_ns::tr;
using Qt::Literals::StringLiterals::operator""_s;

/// @brief Keys allowed in the root object of settings.json.
const QStringList rootKeys = {u"active_preset"_s, u"bands"_s, u"levels"_s,
    u"output"_s, u"playback"_s, u"presets"_s, u"schema_version"_s};
/// @brief Keys allowed in the “playback” object.
const QStringList playbackKeys = {u"autoplay"_s, u"fade_ms"_s, u"sample_rate"_s,
    u"seconds_per_tone"_s};
/// @brief Keys allowed in the “output” object.
const QStringList outputKeys = {u"device"_s, u"master_gain_db"_s};
/// @brief Keys allowed in the “levels” object.
const QStringList levelKeys = {u"max_db"_s, u"min_db"_s, u"step_db"_s};
/// @brief Keys allowed in one band object.
const QStringList bandKeys = {u"frequency_hz"_s, u"level_db"_s};
/// @brief Keys allowed in one preset object.
const QStringList presetKeys = {u"device"_s, u"levels"_s, u"name"_s};

std::optional<QString> asDevice(const QJsonValue &value, const QString &what);
std::optional<QString> asActivePreset(const QJsonValue &value, const QString &what);
QString deviceData(const std::optional<QString> &device);
QString activePresetData(const std::optional<QString> &activePreset);
/// @brief Parses and type-checks the band array.
/// @param value Value to parse.
/// @param what Path of the value for error messages.
/// @return Bands in file order; range validation happens later.
QVector<Band> parseBands(const QJsonValue &value, const QString &what);
/// @brief Parses and type-checks an array of level offsets.
/// @param value Value to parse.
/// @param what Path of the value for error messages.
/// @return Levels in file order; range validation happens later.
QVector<double> parseLevels(const QJsonValue &value, const QString &what);

// -----------------------------------------------------------------------------
// Value validation
// -----------------------------------------------------------------------------

/// @brief Validates one level offset against the slider scale.
/// @param levelDb Level to check, dB.
/// @param scale Allowed range and step.
/// @param what Path of the value for error messages.
void validateLevel(double levelDb, const LevelScale &scale, const QString &what)
{
    if (!std::isfinite(levelDb) || levelDb < scale.minDb || levelDb > scale.maxDb) {
        throw SettingsFormatError(tr("err.levelRange", what, text_format::generalNumber(levelDb),
            text_format::generalNumber(scale.minDb), text_format::generalNumber(scale.maxDb)));
    }
    if (!scale.onStep(levelDb)) {
        throw SettingsFormatError(tr("err.levelStep", what, text_format::generalNumber(levelDb),
            text_format::generalNumber(scale.stepDb)));
    }
}

/// @brief Validates the output device name.
/// @param device Device label; empty means the default device.
/// @param what Path of the value for error messages.
void validateDevice(const std::optional<QString> &device, const QString &what)
{
    if (device.has_value() && device->trimmed().isEmpty()) {
        throw SettingsFormatError(tr("err.deviceEmpty", what));
    }
}

} // namespace

std::optional<QString> presetNameProblem(const QString &name)
{
    if (name.trimmed().isEmpty()) {
        return tr("err.presetNameEmpty");
    }
    if (name != name.trimmed()) {
        return tr("err.presetNameSpaces");
    }
    if (name.length() > maxPresetNameLength) {
        return tr("err.presetNameLong", maxPresetNameLength);
    }
    for (const QChar item : name) {
        if (!item.isPrint()) {
            return tr("err.presetNamePrintable");
        }
    }
    const QString lowered = name.toCaseFolded();
    const QStringList reserved = {u"default"_s};
    if (reserved.contains(lowered) || lowered == u"default"_s) {
        return tr("err.presetNameReserved");
    }
    return std::nullopt;
}

void validateLevelScale(const LevelScale &scale)
{
    const std::array values = {scale.minDb, scale.maxDb, scale.stepDb};
    if (std::any_of(values.begin(), values.end(),
            [](double value) { return !std::isfinite(value); }))
    {
        throw SettingsFormatError(tr("err.levelsFinite"));
    }
    if (!(minLevelScaleDb <= scale.minDb && scale.minDb < 0.0)) {
        throw SettingsFormatError(tr("err.minRange", text_format::generalNumber(minLevelScaleDb)));
    }
    if (!(0.0 < scale.maxDb && scale.maxDb <= maxLevelScaleDb)) {
        throw SettingsFormatError(tr("err.maxRange", text_format::generalNumber(maxLevelScaleDb)));
    }
    if (!(minLevelStepDb <= scale.stepDb && scale.stepDb <= maxLevelStepDb)) {
        throw SettingsFormatError(tr("err.stepRange", text_format::generalNumber(minLevelStepDb),
            text_format::generalNumber(maxLevelStepDb)));
    }
    const double steps = scale.spanDb() / scale.stepDb;
    if (std::abs(steps - std::round(steps)) > stepTolerance) {
        throw SettingsFormatError(tr("err.spanDivisible",
            text_format::generalNumber(scale.spanDb()), text_format::generalNumber(scale.stepDb)));
    }
    if (!scale.onStep(scale.minDb)) {
        throw SettingsFormatError(tr("err.minOnStep", text_format::generalNumber(scale.stepDb)));
    }
}

void validateBands(const QVector<Band> &bands, double maxFrequencyHz, const QString &what,
    const LevelScale &scale)
{
    if (bands.isEmpty() || bands.size() > maxBands) {
        throw SettingsFormatError(tr("err.bandsCount", what, maxBands));
    }

    double previous = 0.0;
    for (int number = 1; number <= bands.size(); ++number) {
        const Band &band = bands[number - 1];
        const double frequency = band.frequencyHz;
        if (!std::isfinite(frequency)
            || !(minFrequencyHz <= frequency && frequency < maxFrequencyHz)) {
            throw SettingsFormatError(tr("err.bandFrequency", what, number,
                text_format::generalNumber(frequency), text_format::generalNumber(minFrequencyHz),
                text_format::generalNumber(maxFrequencyHz)));
        }
        if (number > 1 && frequency <= previous) {
            throw SettingsFormatError(tr("err.bandOrder", what, number,
                text_format::generalNumber(frequency), text_format::generalNumber(previous)));
        }
        previous = frequency;

        validateLevel(band.levelDb, scale,
            what + QStringLiteral(": %1 %2").arg(tr("data.band"), QString::number(number)));
    }
}

void validateSettings(const Settings &settings)
{
    const LevelScale scale = settings.levelScale;
    validateLevelScale(scale);

    if (!(minSecondsPerTone <= settings.secondsPerTone
            && settings.secondsPerTone <= maxSecondsPerTone)) {
        throw SettingsFormatError(tr("err.secondsRange",
            text_format::generalNumber(minSecondsPerTone),
            text_format::generalNumber(maxSecondsPerTone)));
    }
    if (!(minSampleRate <= settings.sampleRate && settings.sampleRate <= maxSampleRate)) {
        throw SettingsFormatError(tr("err.sampleRateRange", minSampleRate, maxSampleRate));
    }
    if (!(minFadeMs <= settings.fadeMs && settings.fadeMs <= maxFadeMs)) {
        throw SettingsFormatError(tr("err.fadeRange", text_format::generalNumber(minFadeMs),
            text_format::generalNumber(maxFadeMs)));
    }
    if (!(masterGainScale.minDb <= settings.masterGainDb
            && settings.masterGainDb <= masterGainScale.maxDb)) {
        throw SettingsFormatError(tr("err.masterRange",
            text_format::generalNumber(masterGainScale.minDb),
            text_format::generalNumber(masterGainScale.maxDb)));
    }
    if (!masterGainScale.onStep(settings.masterGainDb)) {
        throw SettingsFormatError(
            tr("err.masterStep", text_format::generalNumber(masterGainScale.stepDb)));
    }
    validateDevice(settings.device, tr("data.outputDevice"));

    validateBands(settings.bands, nyquistHz(settings.sampleRate), tr("data.bands"), scale);

    QSet<QString> names;
    for (const Preset &preset : settings.presets) {
        const auto problem = presetNameProblem(preset.name);
        if (problem.has_value()) {
            throw SettingsFormatError(tr("err.presetBadName", preset.name, problem.value()));
        }
        const QString key = preset.name.toCaseFolded();
        if (names.contains(key)) {
            throw SettingsFormatError(tr("err.presetDuplicate", preset.name));
        }
        names.insert(key);
        validateDevice(preset.device, tr("data.preset", preset.name));
        if (preset.levels.isEmpty() || preset.levels.size() > maxBands) {
            throw SettingsFormatError(tr("err.presetLevelCount", preset.name, maxBands));
        }
        int number = 1;
        for (const double level : preset.levels) {
            validateLevel(level, scale, tr("err.presetLevel", preset.name, number));
            ++number;
        }
    }

    if (settings.activePreset.has_value()
        && !names.contains(settings.activePreset.value().toCaseFolded())) {
        throw SettingsFormatError(
            tr("err.activePresetMissing", settings.activePreset.value()));
    }
}

// -----------------------------------------------------------------------------
// File parsing
// -----------------------------------------------------------------------------

namespace {

/// @brief Requires a json object.
/// @param value Value to check.
/// @param what Path of the value for error messages.
/// @throws SettingsFormatError If the value is not an object.
QJsonObject asObject(const QJsonValue &value, const QString &what)
{
    if (!value.isObject()) {
        throw SettingsFormatError(tr("err.objectExpected", what));
    }
    return value.toObject();
}

/// @brief Requires exactly the given keys, no more and no less.
/// @param data Object to check.
/// @param keys Expected keys.
/// @param what Path of the object for error messages.
/// @throws SettingsFormatError If keys are missing or unknown.
void exactKeys(const QJsonObject &data, const QStringList &keys, const QString &what)
{
    QStringList missing;
    for (const QString &key : keys) {
        if (!data.contains(key)) {
            missing.push_back(key);
        }
    }
    if (!missing.isEmpty()) {
        missing.sort();
        throw SettingsFormatError(tr("err.keysMissing", what, missing.join(u", "_s)));
    }
    QStringList extra;
    for (auto it = data.begin(); it != data.end(); ++it) {
        if (!keys.contains(it.key())) {
            extra.push_back(it.key());
        }
    }
    if (!extra.isEmpty()) {
        extra.sort();
        throw SettingsFormatError(tr("err.keysUnknown", what, extra.join(u", "_s)));
    }
}

/// @brief Requires a finite json number.
/// @param value Value to check.
/// @param what Path of the value for error messages.
/// @throws SettingsFormatError If the value has the wrong type.
double asNumber(const QJsonValue &value, const QString &what)
{
    if (!value.isDouble()) {
        throw SettingsFormatError(tr("err.numberExpected", what));
    }
    const double number = value.toDouble();
    if (!std::isfinite(number)) {
        throw SettingsFormatError(tr("err.finiteNumber", what));
    }
    return number;
}

/// @brief Requires an integer json number that fits into int.
/// @param value Value to check.
/// @param what Path of the value for error messages.
/// @throws SettingsFormatError If the value has the wrong type.
int asInt(const QJsonValue &value, const QString &what)
{
    if (!value.isDouble()) {
        throw SettingsFormatError(tr("err.intExpected", what));
    }
    const double number = value.toDouble();
    if (tp::exactNe(number, std::floor(number)) || std::abs(number) > 2147483647.0) {
        throw SettingsFormatError(tr("err.intExpected", what));
    }
    return static_cast<int>(number);
}

/// @brief Requires a json boolean.
/// @param value Value to check.
/// @param what Path of the value for error messages.
/// @throws SettingsFormatError If the value has the wrong type.
bool asBool(const QJsonValue &value, const QString &what)
{
    if (!value.isBool()) {
        throw SettingsFormatError(tr("err.boolExpected", what));
    }
    return value.toBool();
}

/// @brief Requires a json string.
/// @param value Value to check.
/// @param what Path of the value for error messages.
/// @throws SettingsFormatError If the value has the wrong type.
QString asText(const QJsonValue &value, const QString &what)
{
    if (!value.isString()) {
        throw SettingsFormatError(tr("err.stringExpected", what));
    }
    return value.toString();
}

/// @brief Parses a device name: a string or the token of the system default device.
/// @param value Value to parse.
/// @param what Path of the value for error messages.
/// @return Device label, or std::nullopt for the default device.
std::optional<QString> asDevice(const QJsonValue &value, const QString &what)
{
    if (value.isString() && value.toString() == defaultDeviceToken) {
        return std::nullopt;
    }
    if (!value.isString()) {
        throw SettingsFormatError(
            tr("err.deviceExpected", what, QByteArray(defaultDeviceToken.data()).constData()));
    }
    return value.toString();
}

/// @brief Parses the active preset: a name or the token meaning “no preset”.
/// @param value Value to parse.
/// @param what Path of the value for error messages.
/// @return Preset name, or std::nullopt if there is no active preset.
std::optional<QString> asActivePreset(const QJsonValue &value, const QString &what)
{
    if (value.isString() && value.toString() == defaultDeviceToken) {
        return std::nullopt;
    }
    if (!value.isString() || value.toString().isEmpty()) {
        throw SettingsFormatError(
            tr("err.activePresetExpected", what, QByteArray(defaultDeviceToken.data()).constData()));
    }
    return value.toString();
}

/// @brief Returns the file representation of the active preset.
/// @param activePreset Preset name, or std::nullopt for “no preset”.
QString activePresetData(const std::optional<QString> &activePreset)
{
    return activePreset.value_or(QString::fromLatin1(defaultDeviceToken.data()));
}

/// @brief Returns the file representation of the output device.
/// @param device Device label, or std::nullopt for the default device.
QString deviceData(const std::optional<QString> &device)
{
    return device.value_or(QString::fromLatin1(defaultDeviceToken.data()));
}

QVector<Band> parseBands(const QJsonValue &value, const QString &what)
{
    if (!value.isArray()) {
        throw SettingsFormatError(tr("err.listExpected", what));
    }
    QVector<Band> bands;
    const QJsonArray items = value.toArray();
    for (int number = 1; number <= items.size(); ++number) {
        const QString label = what + QStringLiteral("[%1]").arg(number);
        const QJsonObject data = asObject(items.at(number - 1), label);
        exactKeys(data, bandKeys, label);
        bands.push_back(Band{
            asNumber(data.value(u"frequency_hz"_s), label + u".frequency_hz"_s),
            asNumber(data.value(u"level_db"_s), label + u".level_db"_s),
        });
    }
    return bands;
}

QVector<double> parseLevels(const QJsonValue &value, const QString &what)
{
    if (!value.isArray()) {
        throw SettingsFormatError(tr("err.listExpected", what));
    }
    QVector<double> levels;
    const QJsonArray items = value.toArray();
    for (int number = 1; number <= items.size(); ++number) {
        levels.push_back(asNumber(items.at(number - 1), what + QStringLiteral("[%1]").arg(number)));
    }
    return levels;
}

} // namespace

Settings parseSettings(const QJsonObject &root)
{
    const QJsonValue versionValue = root.value(u"schema_version"_s);
    if (versionValue.isDouble()
        && tp::exactEq(versionValue.toDouble(), std::floor(versionValue.toDouble())))
    {
        const auto version = static_cast<int>(versionValue.toDouble());
        if (version != schemaVersion) {
            throw SettingsVersionError(tr("err.settingsVersion", version, schemaVersion));
        }
    }
    exactKeys(root, rootKeys, tr("data.file"));
    static_cast<void>(asInt(versionValue, u"schema_version"_s));

    const QJsonObject playback = asObject(root.value(u"playback"_s), u"playback"_s);
    exactKeys(playback, playbackKeys, u"playback"_s);
    const QJsonObject output = asObject(root.value(u"output"_s), u"output"_s);
    exactKeys(output, outputKeys, u"output"_s);
    const QJsonObject levels = asObject(root.value(u"levels"_s), u"levels"_s);
    exactKeys(levels, levelKeys, u"levels"_s);

    QVector<Preset> presets;
    const QJsonValue presetsValue = root.value(u"presets"_s);
    if (!presetsValue.isArray()) {
        throw SettingsFormatError(tr("err.listExpected", u"presets"_s));
    }
    const QJsonArray presetItems = presetsValue.toArray();
    for (int number = 1; number <= presetItems.size(); ++number) {
        const QString label = QStringLiteral("presets[%1]").arg(number);
        const QJsonObject data = asObject(presetItems.at(number - 1), label);
        exactKeys(data, presetKeys, label);
        presets.push_back(Preset{
            asText(data.value(u"name"_s), label + u".name"_s),
            asDevice(data.value(u"device"_s), label + u".device"_s),
            parseLevels(data.value(u"levels"_s), label + u".levels"_s),
        });
    }

    Settings settings;
    settings.secondsPerTone =
        asNumber(playback.value(u"seconds_per_tone"_s), u"playback.seconds_per_tone"_s);
    settings.sampleRate = asInt(playback.value(u"sample_rate"_s), u"playback.sample_rate"_s);
    settings.fadeMs = asNumber(playback.value(u"fade_ms"_s), u"playback.fade_ms"_s);
    settings.autoplay = asBool(playback.value(u"autoplay"_s), u"playback.autoplay"_s);
    settings.masterGainDb =
        asNumber(output.value(u"master_gain_db"_s), u"output.master_gain_db"_s);
    settings.device = asDevice(output.value(u"device"_s), u"output.device"_s);
    settings.bands = parseBands(root.value(u"bands"_s), u"bands"_s);
    settings.presets = presets;
    settings.activePreset = asActivePreset(root.value(u"active_preset"_s), u"active_preset"_s);
    settings.levelScale = LevelScale{
        asNumber(levels.value(u"min_db"_s), u"levels.min_db"_s),
        asNumber(levels.value(u"max_db"_s), u"levels.max_db"_s),
        asNumber(levels.value(u"step_db"_s), u"levels.step_db"_s),
    };
    validateSettings(settings);
    return settings;
}

Settings loadSettings(const std::filesystem::path &path)
{
    const QString pathText = QString::fromStdString(path.string());
    if (!QFileInfo::exists(pathText) || !QFileInfo(pathText).isFile()) {
        throw SettingsNotFoundError(pathText);
    }

    QFile file(pathText);
    if (!file.open(QIODeviceBase::ReadOnly)) {
        throw SettingsReadError(tr("err.settingsRead", file.errorString()));
    }
    QByteArray bytes = file.readAll();
    file.close();
    // utf-8-sig: the byte order mask is allowed.
    if (bytes.startsWith("\xef\xbb\xbf")) {
        bytes.remove(0, 3);
    }

    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString text = decoder.decode(bytes);
    if (decoder.hasError()) {
        throw SettingsFormatError(tr("err.settingsEncoding"));
    }

    QJsonParseError parseError = {};
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        throw SettingsFormatError(tr("err.settingsJson", parseError.errorString()));
    }
    if (!document.isObject()) {
        throw SettingsFormatError(tr("err.fileObject"));
    }
    return parseSettings(document.object());
}

// -----------------------------------------------------------------------------
// File writing
// -----------------------------------------------------------------------------



QString scalarText(double value)
{
    return text_format::pythonFloat(value);
}

namespace {

/// @brief Returns a json string literal: quotes and escapes the text.
/// @param text Raw text.
QString scalarText(const QString &text)
{
    QString escaped = text;
    escaped.replace(u'\\', u"\\\\"_s);
    escaped.replace(u'"', u"\\\""_s);
    return u'"' + escaped + u'"';
}

/// @brief Formats a json value with two-space indentation.
///
/// Simple objects inside lists (bands and presets) are written on one line,
/// matching the layout of the original program.
/// @param value Value to format.
/// @param indent Current indentation, spaces.
/// @param inlineObjects Whether a flat object may stay on one line.
QString formatValue(const QJsonValue &value, int indent, bool inlineObjects)
{
    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        if (object.isEmpty()) {
            return u"{}"_s;
        }
        // Inline form: the object contains no nested containers.
        if (inlineObjects && std::none_of(object.begin(), object.end(),
                [](const QJsonValueConstRef it) {
                    return it.isObject() || it.isArray();
                }))
        {
            QStringList pairs;
            for (auto it = object.begin(); it != object.end(); ++it) {
                pairs.push_back(scalarText(it.key()) + u": "_s
                    + formatValue(it.value(), indent, false));
            }
            return u"{"_s + pairs.join(u", "_s) + u"}"_s;
        }
        const QString pad = QString(u' ').repeated(indent + 2);
        const QString end = QString(u' ').repeated(indent);
        QStringList lines;
        for (auto it = object.begin(); it != object.end(); ++it) {
            lines.push_back(pad + scalarText(it.key()) + u": "_s
                + formatValue(it.value(), indent + 2, false));
        }
        return u"{\n"_s + lines.join(u",\n"_s) + u'\n' + end + u'}';
    }
    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        if (array.isEmpty()) {
            return u"[]"_s;
        }
        const QString pad = QString(u' ').repeated(indent + 2);
        const QString end = QString(u' ').repeated(indent);
        QStringList lines;
        for (const QJsonValue &item : array) {
            // Elements of the list — bands — are inline.
            lines.push_back(pad + formatValue(item, indent + 2, true));
        }
        return u"[\n"_s + lines.join(u",\n"_s) + u'\n' + end + u']';
    }
    if (value.isString()) {
        return scalarText(value.toString());
    }
    if (value.isBool()) {
        return value.toBool() ? u"true"_s : u"false"_s;
    }
    if (value.isDouble()) {
        return text_format::pythonFloat(value.toDouble());
    }
    return u"null"_s;
}

} // namespace

QString settingsToText(const Settings &settings)
{
    QJsonArray bands;
    // In bands the levels are always 0.0: levels are stored exclusively in presets.
    for (const Band &band : settings.bands) {
        bands.push_back(QJsonObject{
            {u"frequency_hz"_s, band.frequencyHz},
            {u"level_db"_s, 0.0},
        });
    }

    QJsonArray presets;
    for (const Preset &preset : settings.presets) {
        QJsonArray levels;
        for (const double level : preset.levels) {
            levels.push_back(level);
        }
        presets.push_back(QJsonObject{
            {u"name"_s, preset.name},
            {u"device"_s, deviceData(preset.device)},
            {u"levels"_s, levels},
        });
    }

    const QJsonObject root{
        {u"schema_version"_s, schemaVersion},
        {u"playback"_s,
            QJsonObject{
                {u"seconds_per_tone"_s, settings.secondsPerTone},
                {u"sample_rate"_s, settings.sampleRate},
                {u"fade_ms"_s, settings.fadeMs},
                {u"autoplay"_s, settings.autoplay},
            }},
        {u"output"_s,
            QJsonObject{
                {u"device"_s, deviceData(settings.device)},
                {u"master_gain_db"_s, settings.masterGainDb},
            }},
        {u"levels"_s,
            QJsonObject{
                {u"min_db"_s, settings.levelScale.minDb},
                {u"max_db"_s, settings.levelScale.maxDb},
                {u"step_db"_s, settings.levelScale.stepDb},
            }},
        {u"bands"_s, bands},
        {u"presets"_s, presets},
        {u"active_preset"_s, activePresetData(settings.activePreset)},
    };
    return formatValue(root, 0, false) + u'\n';
}

void saveSettings(const std::filesystem::path &path, const Settings &settings)
{
    // In the saved file the bands always have zero level.
    Settings toSave = settings;
    for (Band &band : toSave.bands) {
        band.levelDb = 0.0;
    }

    try {
        validateSettings(toSave);
    } catch (const SettingsError &error) {
        throw SettingsSaveError(tr("save.notWritten", error.message()));
    }

    const QString text = settingsToText(toSave);
    try {
        QJsonParseError parseError = {};
        const QJsonDocument document = QJsonDocument::fromJson(text.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            throw SettingsFormatError(tr("err.settingsJson", parseError.errorString()));
        }
        if (parseSettings(document.object()) != toSave) {
            throw SettingsSaveError(tr("save.mismatch"));
        }
    } catch (const SettingsError &error) {
        throw SettingsSaveError(tr("save.notWritten", error.message()));
    }

    const QString target = QString::fromStdString(path.string());
    // Atomic write: temporary file nearby, then replace.
    QSaveFile file(target);
    if (!file.open(QIODeviceBase::WriteOnly)) {
        throw SettingsSaveError(tr("save.writeFailed", file.errorString()));
    }
    const QByteArray bytes = text.toUtf8();
    if (file.write(bytes) != bytes.size() || !file.flush()) {
        file.cancelWriting();
        throw SettingsSaveError(tr("save.writeFailed", file.errorString()));
    }
    if (!file.commit()) {
        throw SettingsSaveError(tr("save.writeFailed", file.errorString()));
    }
}

std::filesystem::path quarantineCorrupted(const std::filesystem::path &path)
{
    const QDateTime now = QDateTime::currentDateTime();
    const QString stamp = now.toString(u"yyyyMMdd-hhmmss"_s);
    const auto parent = path.parent_path();
    const QString stem = QString::fromStdString(path.stem().string());
    const QString suffix = QString::fromStdString(path.extension().string());
    QString name = QStringLiteral("%1.corrupted-%2%3").arg(stem, stamp, suffix);
    int counter = 1;
    std::error_code code;
    auto target = parent / name.toStdString();
    while (std::filesystem::exists(target, code)) {
        ++counter;
        name = QStringLiteral("%1.corrupted-%2-%3%4")
                   .arg(stem, stamp, QString::number(counter), suffix);
        target = parent / name.toStdString();
    }
    std::filesystem::rename(path, target, code);
    if (code) {
        throw SettingsSaveError(tr("quarantine.renameFailed", code.message().c_str()));
    }
    return target;
}

Settings loadSettingsAtStartup(const std::filesystem::path &path)
{
    try {
        return loadSettings(path);
    } catch (const SettingsFormatError &error) {
        try {
            const auto target = quarantineCorrupted(path);
            throw SettingsFormatError(tr("quarantine.message", error.message(),
                QString::fromStdString(target.filename().string()),
                QString::fromStdString(path.filename().string())));
        } catch (const SettingsSaveError &renameError) {
            throw SettingsFormatError(error.message() + u"\n\n"_s + renameError.message());
        }
    }
}

} // namespace tp::settings_store
