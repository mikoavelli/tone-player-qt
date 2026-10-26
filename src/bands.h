// Equalizer logic without Qt: a set of bands and reading alignment.
#pragma once

#include "models.h"

#include <QMap>

namespace tp::bands {

// Frequencies are considered equal if they differ by less than this value, Hz.
inline constexpr double sameFrequencyHz = 1e-6;

// Uniform frequency grid; all level offsets are zero.
QVector<Band> generateGrid(double startHz, double stepHz, int count, double maxFrequencyHz,
    const LevelScale &scale = defaultLevelScale);
// Add a band with a zero offset at its place by frequency.
QVector<Band> insertBand(const QVector<Band> &bands, double frequencyHz,
    double maxFrequencyHz, const LevelScale &scale = defaultLevelScale);
// Remove the band; the last band always stays.
QVector<Band> removeBand(const QVector<Band> &bands, int index);
// Change the band frequency without reordering; the offset stays with it.
QVector<Band> setFrequency(const QVector<Band> &bands, int index, double frequencyHz,
    double maxFrequencyHz, const LevelScale &scale = defaultLevelScale);
// The same frequencies with new level offsets.
QVector<Band> withLevels(const QVector<Band> &bands, const QVector<double> &levels);
// The same frequencies with zero offsets.
QVector<Band> resetLevels(const QVector<Band> &bands);

// A sound level meter reading for one band.
//
// levelDbfs is the level of the signal at which the reading was taken: the
// overall level plus the band offset, but not above 0 dB (above full scale the
// signal is clipped). Therefore the readings stay valid even if the sliders
// were moved after the measurement.
struct Reading {
    double measuredDb = 0.0;
    double levelDbfs = 0.0;
};

// Result of flattening.
struct FlattenResult {
    QVector<double> levels;
    int referenceIndex = 0;
    double spreadDb = 0.0;
    QVector<int> changed;
    QVector<int> clamped;
};

// Adjust the offsets so that the pressure on all measured frequencies matches.
FlattenResult flatten(const QVector<double> &levels, const QMap<int, Reading> &readings,
    double masterDb, const LevelScale &scale = defaultLevelScale);

} // namespace tp::bands
