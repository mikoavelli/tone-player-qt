// Settings dialog: sliders, tone and frequencies of the bands.
#pragma once

#include "models.h"

#include <QDoubleSpinBox>
#include <QDialog>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <optional>

class QWidget;

namespace tp::ui {

// Width of the labels is the same in all sections: fields line up exactly.
inline constexpr int labelWidth = 190;
inline constexpr int buttonHeight = 34;

class SettingsDialog final : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(const Settings &settings, QWidget *parent = nullptr);

    // Validated result. Throws std::logic_error if not accepted yet.
    [[nodiscard]] const Settings &settings() const;

private:
    void build();
    void accept() final;

    void resetScale();
    void resetLevels();
    [[nodiscard]] LevelScale scale() const;
    void apply();

    Settings dialogSettings_;
    QVector<Band> bands_;
    std::optional<Settings> result_;

    QDoubleSpinBox *minSpin_ = nullptr;
    QDoubleSpinBox *maxSpin_ = nullptr;
    QDoubleSpinBox *stepSpin_ = nullptr;
    QDoubleSpinBox *secondsSpin_ = nullptr;
    QDoubleSpinBox *fadeSpin_ = nullptr;
    QSpinBox *rateSpin_ = nullptr;
    QPlainTextEdit *frequenciesEdit_ = nullptr;
};

// Frequency list text: numbers, separated by commas.
QString frequenciesText(const QVector<Band> &bands);
// Parse the numbers; std::nullopt on any error.
std::optional<QVector<double>> parseFrequencies(const QString &text);

} // namespace tp::ui
