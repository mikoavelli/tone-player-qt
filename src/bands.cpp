/// @file bands.cpp
/// @brief Equalizer logic without Qt: a set of bands and reading alignment.
///
/// Implementation of the band operations declared in bands.h.
#include "bands.h"

#include "errors.h"
#include "settings_store.h"
#include "errors.h"
#include "exact_eq.h"
#include "text_format.h"

#include <QSet>
#include <algorithm>
#include <map>

namespace tp::bands {

using ::BandError;

namespace {

using tr_ns::tr;

/// @brief Rounds a frequency to thousandths of a hertz, as in the original grid generator.
/// @param value Frequency, Hz.
double tnsRound(double value)
{
    return std::round(value * 1000.0) / 1000.0;
}

/// @brief Validates a band list and converts settings errors to band errors.
/// @param bands Bands to validate.
/// @param maxFrequencyHz Highest allowed frequency, Hz.
/// @param scale Range and step of the level offsets.
/// @return The validated bands.
/// @throws BandError If the bands are invalid.
QVector<Band> checked(const QVector<Band> &bands, double maxFrequencyHz,
    const LevelScale &scale)
{
    try {
        settings_store::validateBands(bands, maxFrequencyHz, tr("data.bands"), scale);
    } catch (const SettingsError &error) {
        throw BandError(error.message());
    }
    return bands;
}

} // namespace

QVector<Band> generateGrid(double startHz, double stepHz, int count, double maxFrequencyHz,
    const LevelScale &scale)
{
    if (!std::isfinite(startHz) || !std::isfinite(stepHz)) {
        throw BandError(tr("err.bandGrid"));
    }
    if (stepHz <= 0.0) {
        throw BandError(tr("err.bandGridStep"));
    }
    if (count < 1 || count > maxBands) {
        throw BandError(tr("err.bandsCount", tr("data.bands"), maxBands));
    }
    QVector<Band> bands;
    bands.reserve(count);
    for (int index = 0; index < count; ++index) {
        bands.push_back(Band{tnsRound(startHz + stepHz * index), 0.0});
    }
    return checked(bands, maxFrequencyHz, scale);
}

QVector<Band> insertBand(const QVector<Band> &bands, double frequencyHz,
    double maxFrequencyHz, const LevelScale &scale)
{
    for (const Band &band : bands) {
        if (std::abs(band.frequencyHz - frequencyHz) < sameFrequencyHz) {
            throw BandError(tr("err.bandExists", text_format::generalNumber(frequencyHz)));
        }
    }
    QVector<Band> merged = bands;
    merged.push_back(Band{frequencyHz, 0.0});
    std::sort(merged.begin(), merged.end(),
        [](const Band &left, const Band &right) { return left.frequencyHz < right.frequencyHz; });
    return checked(merged, maxFrequencyHz, scale);
}

QVector<Band> removeBand(const QVector<Band> &bands, int index)
{
    if (index < 0 || index >= bands.size()) {
        throw BandError(tr("err.bandNotFound", index + 1));
    }
    if (bands.size() == 1) {
        throw BandError(tr("err.bandLast"));
    }
    QVector<Band> result;
    result.reserve(bands.size() - 1);
    for (int position = 0; position < bands.size(); ++position) {
        if (position != index) {
            result.push_back(bands.at(position));
        }
    }
    return result;
}

QVector<Band> setFrequency(const QVector<Band> &bands, int index, double frequencyHz,
    double maxFrequencyHz, const LevelScale &scale)
{
    if (index < 0 || index >= bands.size()) {
        throw BandError(tr("err.bandNotFound", index + 1));
    }
    QVector<Band> changed = bands;
    changed[index].frequencyHz = tnsRound(frequencyHz);
    return checked(changed, maxFrequencyHz, scale);
}

QVector<Band> withLevels(const QVector<Band> &bands, const QVector<double> &levels)
{
    if (levels.size() != bands.size()) {
        throw BandError(tr("err.bandLevelsCount"));
    }
    QVector<Band> result;
    result.reserve(bands.size());
    for (int index = 0; index < bands.size(); ++index) {
        result.push_back(Band{bands.at(index).frequencyHz, levels.at(index)});
    }
    return result;
}

QVector<Band> resetLevels(const QVector<Band> &bands)
{
    QVector<Band> result;
    result.reserve(bands.size());
    for (const Band &band : bands) {
        result.push_back(Band{band.frequencyHz, 0.0});
    }
    return result;
}

FlattenResult flatten(const QVector<double> &levels, const QMap<int, Reading> &readings,
    double masterDb, const LevelScale &scale)
{
    if (readings.size() < 2) {
        throw BandError(tr("err.flattenNeed"));
    }
    for (auto it = readings.begin(); it != readings.end(); ++it) {
        if (it.key() < 0 || it.key() >= levels.size()) {
            throw BandError(tr("err.bandNotFound", it.key() + 1));
        }
        if (!std::isfinite(it.value().measuredDb) || !std::isfinite(it.value().levelDbfs)) {
            throw BandError(tr("err.flattenInfinite", it.key() + 1));
        }
    }

    std::map<int, double> sensitivity;
    for (auto it = readings.begin(); it != readings.end(); ++it) {
        sensitivity[it.key()] = it.value().measuredDb - it.value().levelDbfs;
    }
    // The support is the "quietest" band.
    int reference = sensitivity.begin()->first;
    for (const auto &[index, value] : sensitivity) {
        if (value < sensitivity.at(reference)
            || (tp::exactEq(value, sensitivity.at(reference)) && index < reference))
        {
            reference = index;
        }
    }
    const double anchor = std::min(masterDb + levels.at(reference), 0.0);

    QVector<double> newLevels = levels;
    QVector<int> clamped;
    for (const auto &[index, value] : sensitivity) {
        const double wantedTotal = anchor + sensitivity.at(reference) - sensitivity.at(index);
        const double snapped = scale.snap(wantedTotal - masterDb);
        const double level = std::clamp(snapped, scale.minDb, scale.maxDb);
        if (tp::exactNe(level, snapped)) {
            clamped.push_back(index);
        }
        newLevels[index] = level;
    }

    QVector<int> changed;
    for (const int index : readings.keys()) {
        if (tp::exactNe(newLevels.at(index), levels.at(index))) {
            changed.push_back(index);
        }
    }

    const double maxSensitivity = std::max_element(sensitivity.begin(), sensitivity.end(),
        [](const auto &left, const auto &right) { return left.second < right.second; })
                                     ->second;
    return {
        newLevels, reference, maxSensitivity - sensitivity.at(reference), changed, clamped};
}

} // namespace tp::bands
