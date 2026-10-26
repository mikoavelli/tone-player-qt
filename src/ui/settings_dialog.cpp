// Settings dialog: sliders, tone and frequencies of the bands.
#include "settings_dialog.h"

#include "errors.h"
#include "i18n.h"
#include "settings_store.h"
#include "text_format.h"

#include <QBoxLayout>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFontDatabase>
#include <QLabel>
#include <QMessageBox>
#include <QRegularExpression>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStringList>
#include <QtGlobal>
#include <stdexcept>
#include <utility>

namespace tp::ui {

using tr_ns::tr;
using Qt::Literals::StringLiterals::operator""_s;

namespace {

// Section title.
QLabel *sectionLabel(const QString &text)
{
    auto *label = new QLabel(text);
    label->setObjectName(u"sectionLabel"_s);
    return label;
}

QFormLayout *makeForm()
{
    auto *form = new QFormLayout();
    form->setSpacing(8);
    form->setContentsMargins(0, 0, 0, 0);
    form->setLabelAlignment(
        Qt::AlignmentFlag::AlignLeft | Qt::AlignmentFlag::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    return form;
}

void addRow(QFormLayout &form, const QString &text, QWidget &field)
{
    auto *label = new QLabel(text);
    label->setObjectName(u"fieldLabel"_s);
    label->setFixedWidth(labelWidth);
    field.setSizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Fixed);
    form.addRow(label, &field);
}

QDoubleSpinBox *doubleBox(double value, double minimum, double maximum, const QString &suffix,
    int decimals = 1, double step = 0.5)
{
    auto *box = new QDoubleSpinBox();
    box->setRange(minimum, maximum);
    box->setDecimals(decimals);
    box->setSingleStep(step);
    box->setSuffix(suffix);
    box->setValue(value);
    return box;
}

QPushButton *resetButton(const QString &text, const QString &tooltip)
{
    auto *button = new QPushButton(text);
    button->setToolTip(tooltip);
    button->setFixedHeight(buttonHeight);
    button->setCursor(Qt::CursorShape::PointingHandCursor);
    return button;
}

} // namespace

// Frequency list text: numbers, separated by commas.
QString frequenciesText(const QVector<Band> &bands)
{
    QStringList texts;
    texts.reserve(bands.size());
    for (const Band &band : bands) {
        texts.push_back(text_format::generalNumber(band.frequencyHz));
    }
    return texts.join(u", "_s);
}

// Parse the numbers; std::nullopt on any error.
std::optional<QVector<double>> parseFrequencies(const QString &text)
{
    const QRegularExpression separator(u"[\\s,;]+"_s);
    QStringList parts;
    for (const QString &part : text.trimmed().split(separator, Qt::SkipEmptyParts)) {
        parts.push_back(part.trimmed());
    }
    if (parts.isEmpty()) {
        return std::nullopt;
    }
    QVector<double> values;
    for (const QString &part : parts) {
        bool ok = false;
        const double value = QString(part).replace(u',', u'.').toDouble(&ok);
        if (!ok || !std::isfinite(value)) {
            return std::nullopt;
        }
        values.push_back(value);
    }
    return values;
}

SettingsDialog::SettingsDialog(const Settings &settings, QWidget *parent)
    : QDialog(parent)
    , dialogSettings_(settings)
    , bands_(settings.bands)
{
    setWindowTitle(tr_ns::tr("settings.title"));
    setMinimumWidth(480);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(10);

    // Level scale.
    root->addWidget(sectionLabel(tr_ns::tr("settings.section.equalizer")));
    auto *form = makeForm();
    const LevelScale scaleValues = settings.levelScale;
    minSpin_ = doubleBox(scaleValues.minDb, minLevelScaleDb, 0.0, tr_ns::tr("unit.db"));
    maxSpin_ = doubleBox(scaleValues.maxDb, 0.0, maxLevelScaleDb, tr_ns::tr("unit.db"));
    stepSpin_ = doubleBox(
        scaleValues.stepDb, minLevelStepDb, maxLevelStepDb, tr_ns::tr("unit.db"), 2, 0.1);
    addRow(*form, tr_ns::tr("settings.field.min"), *minSpin_);
    addRow(*form, tr_ns::tr("settings.field.max"), *maxSpin_);
    addRow(*form, tr_ns::tr("settings.field.step"), *stepSpin_);
    root->addLayout(form);

    auto *scaleButtons = new QHBoxLayout();
    scaleButtons->addStretch(1);
    auto *scaleReset = resetButton(tr_ns::tr("settings.resetScale"), tr_ns::tr("settings.resetScale.tooltip"));
    connect(scaleReset, &QPushButton::clicked, this, &SettingsDialog::resetScale);
    scaleButtons->addWidget(scaleReset);
    root->addLayout(scaleButtons);

    // Tone.
    root->addWidget(sectionLabel(tr_ns::tr("settings.section.tone")));
    auto *toneForm = makeForm();
    secondsSpin_ = doubleBox(
        settings.secondsPerTone, minSecondsPerTone, maxSecondsPerTone, tr_ns::tr("unit.s"), 1, 1.0);
    fadeSpin_ = doubleBox(
        settings.fadeMs, minFadeMs, maxFadeMs, tr_ns::tr("unit.ms"), 0, 10.0);
    rateSpin_ = new QSpinBox(this);
    rateSpin_->setRange(minSampleRate, maxSampleRate);
    rateSpin_->setSingleStep(100);
    rateSpin_->setSuffix(tr_ns::tr("unit.hz"));
    rateSpin_->setValue(settings.sampleRate);
    addRow(*toneForm, tr_ns::tr("settings.field.seconds"), *secondsSpin_);
    addRow(*toneForm, tr_ns::tr("settings.field.fade"), *fadeSpin_);
    addRow(*toneForm, tr_ns::tr("settings.field.sampleRate"), *rateSpin_);
    root->addLayout(toneForm);

    // Frequencies.
    root->addWidget(sectionLabel(tr_ns::tr("settings.section.frequencies")));
    frequenciesEdit_ = new QPlainTextEdit(frequenciesText(settings.bands), this);
    frequenciesEdit_->setFixedHeight(92);
    frequenciesEdit_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    frequenciesEdit_->setToolTip(tr_ns::tr("settings.frequencies.tooltip"));
    root->addWidget(frequenciesEdit_);
    auto *hint = new QLabel(tr_ns::tr("settings.frequencies.hint"), this);
    hint->setObjectName(u"subtitleLabel"_s);
    hint->setWordWrap(true);
    root->addWidget(hint);

    auto *levelsRow = new QHBoxLayout();
    levelsRow->addStretch(1);
    auto *levelsReset =
        resetButton(tr_ns::tr("settings.resetLevels"), tr_ns::tr("settings.resetLevels.tooltip"));
    connect(levelsReset, &QPushButton::clicked, this, &SettingsDialog::resetLevels);
    levelsRow->addWidget(levelsReset);
    root->addLayout(levelsRow);

    root->addSpacing(6);

    // Buttons.
    auto *buttons = new QHBoxLayout();
    buttons->setSpacing(8);
    buttons->addStretch(1);
    auto *cancel = new QPushButton(tr_ns::tr("dialog.cancel"), this);
    cancel->setFixedHeight(38);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    auto *save = new QPushButton(tr_ns::tr("dialog.save"), this);
    save->setObjectName(u"playButton"_s);
    save->setFixedHeight(38);
    save->setDefault(true);
    connect(save, &QPushButton::clicked, this, &SettingsDialog::apply);
    buttons->addWidget(cancel);
    buttons->addWidget(save);
    root->addLayout(buttons);
}

const Settings &SettingsDialog::settings() const
{
    if (!result_.has_value()) {
        throw std::logic_error("The settings are not confirmed yet.");
    }
    return result_.value();
}

void SettingsDialog::resetScale()
{
    minSpin_->setValue(-10.0);
    maxSpin_->setValue(10.0);
    stepSpin_->setValue(0.5);
}

void SettingsDialog::resetLevels()
{
    for (Band &band : bands_) {
        band.levelDb = 0.0;
    }
}

LevelScale SettingsDialog::scale() const
{
    return LevelScale{minSpin_->value(), maxSpin_->value(), stepSpin_->value()};
}

void SettingsDialog::apply()
{
    const auto frequencies = parseFrequencies(frequenciesEdit_->toPlainText());
    if (!frequencies.has_value()) {
        QMessageBox::warning(
            this, tr_ns::tr("settings.badFrequencies.title"), tr_ns::tr("settings.badFrequencies.text"));
        return;
    }

    const LevelScale newScale = scale();
    const QVector<Band> oldBands = bands_;
    QVector<Band> newBands;
    newBands.reserve(frequencies->size());
    for (int index = 0; index < frequencies->size(); ++index) {
        const double levelDb =
            index < oldBands.size() ? oldBands.at(index).levelDb : 0.0;
        newBands.push_back(Band{frequencies->at(index), newScale.clamp(levelDb)});
    }

    Settings candidate = dialogSettings_;
    candidate.secondsPerTone = secondsSpin_->value();
    candidate.sampleRate = rateSpin_->value();
    candidate.fadeMs = fadeSpin_->value();
    candidate.levelScale = newScale;
    candidate.bands = newBands;
    for (Preset &preset : candidate.presets) {
        for (double &level : preset.levels) {
            level = newScale.clamp(level);
        }
    }

    try {
        settings_store::validateSettings(candidate);
    } catch (const SettingsError &error) {
        QMessageBox::warning(
            this, tr_ns::tr("settings.badSettings.title"), error.message());
        return;
    }
    result_ = candidate;
    accept();
}

void SettingsDialog::accept()
{
    QDialog::accept();
}

} // namespace tp::ui
