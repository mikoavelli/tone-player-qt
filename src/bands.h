/// @file bands.h
/// @brief Equalizer logic without Qt: a set of bands and reading alignment.
#pragma once

#include "models.h"

#include <QMap>

/// @brief Operations on equalizer bands and level measurement readings.
namespace tp::bands {

/// @brief Frequencies are considered equal if they differ by less than this value, Hz.
inline constexpr double sameFrequencyHz = 1e-6;

/// @brief Builds a uniform frequency grid with all level offsets at zero.
/// @param startHz First frequency, Hz.
/// @param stepHz Distance between neighbouring frequencies, Hz.
/// @param count Number of bands.
/// @param maxFrequencyHz Upper bound for the generated frequencies, Hz.
/// @param scale Range and step used to snap the zero offsets.
/// @return Bands in ascending frequency order.
QVector<Band> generateGrid(double startHz, double stepHz, int count, double maxFrequencyHz,
    const LevelScale &scale = defaultLevelScale);

/// @brief Adds a band with a zero offset at its place by frequency.
/// @param bands Existing bands in ascending order.
/// @param frequencyHz Frequency of the new band, Hz.
/// @param maxFrequencyHz Upper bound for the frequency, Hz.
/// @param scale Range and step used to snap the zero offset.
/// @return Bands with the new one inserted; throws BandError if the limit is reached.
QVector<Band> insertBand(const QVector<Band> &bands, double frequencyHz,
    double maxFrequencyHz, const LevelScale &scale = defaultLevelScale);

/// @brief Removes the band; the last band always stays.
/// @param bands Existing bands in ascending order.
/// @param index Index of the band to remove.
/// @return Bands without the requested one.
QVector<Band> removeBand(const QVector<Band> &bands, int index);

/// @brief Changes the band frequency without reordering; the offset stays with it.
/// @param bands Existing bands in ascending order.
/// @param index Index of the band to change.
/// @param frequencyHz New frequency, Hz.
/// @param maxFrequencyHz Upper bound for the frequency, Hz.
/// @param scale Range and step used to snap the level.
/// @return Bands with the frequency replaced; throws BandError on a collision or a bad value.
QVector<Band> setFrequency(const QVector<Band> &bands, int index, double frequencyHz,
    double maxFrequencyHz, const LevelScale &scale = defaultLevelScale);

/// @brief Returns the same frequencies with new level offsets.
/// @param bands Source bands.
/// @param levels Level offsets in band order, dB.
/// @return Bands with the offsets replaced.
QVector<Band> withLevels(const QVector<Band> &bands, const QVector<double> &levels);

/// @brief Returns the same frequencies with zero offsets.
/// @param bands Source bands.
/// @return Bands with every level offset set to zero.
QVector<Band> resetLevels(const QVector<Band> &bands);

/// @brief A sound level meter reading for one band.
///
/// @ref levelDbfs is the level of the signal at which the reading was taken: the
/// overall level plus the band offset, but not above 0 dB (above full scale the
/// signal is clipped). Therefore the readings stay valid even if the sliders
/// were moved after the measurement.
struct Reading {
    double measuredDb = 0.0; ///< Measured pressure of the band, dB.
    double levelDbfs = 0.0; ///< Signal level at measurement time, dB from full scale.
};

/// @brief Result of flattening.
struct FlattenResult {
    QVector<double> levels; ///< New band level offsets, dB.
    int referenceIndex = 0; ///< Band whose level is taken as the reference.
    double spreadDb = 0.0; ///< Remaining spread of the readings, dB.
    QVector<int> changed; ///< Indices of the bands whose level changed.
    QVector<int> clamped; ///< Indices of the bands limited by the scale range.
};

/// @brief Adjusts the offsets so that the pressure on all measured frequencies matches.
/// @param levels Current band level offsets, dB.
/// @param readings Measured readings by band index.
/// @param masterDb Current overall level, dB from full scale.
/// @param scale Range and step of the level offsets.
/// @return New offsets and details of the adjustment.
FlattenResult flatten(const QVector<double> &levels, const QMap<int, Reading> &readings,
    double masterDb, const LevelScale &scale = defaultLevelScale);

} // namespace tp::bands
