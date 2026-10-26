/// @file text_format.cpp
/// @brief Formatting numbers for the interface.
///
/// Implementation of the formatting helpers declared in text_format.h.
#include "exact_eq.h"
#include "text_format.h"

#include "i18n.h"

#include <QChar>
#include <QtGlobal>
#include <QtMath>
#include <charconv>
#include <cmath>
#include <limits>
#include <system_error>

namespace text_format {

using tr_ns::tr;

namespace {

/// @brief Unicode minus sign used instead of the hyphen.
constexpr char16_t minusSign = u'\u2212';
/// @brief Size of the stack buffer used to format numbers.
constexpr qsizetype numberBuffer = 64;

/// @brief Inserts spaces between thousands, like in the original: "10 000".
/// @param value Integer to format.
QString grouped(qint64 value)
{
    QString digits = QString::number(value);
    if (digits.size() > 3) {
        for (qsizetype position = digits.size() - 3;
             position > (digits.startsWith(u'-') ? 1 : 0);
             position -= 3) {
            digits.insert(position, u' ');
        }
    }
    return digits;
}

/// @brief Returns a short representation in the style of python repr: "0.5", "-10", "1e-05".
///
/// Needed for the text of settings.json.
/// @param value Number to format.
QString makeShortFloat(double value)
{
    char buffer[numberBuffer];
    const auto result = std::to_chars(buffer, buffer + numberBuffer, value);
    if (result.ec != std::errc()) {
        return QStringLiteral("0");
    }
    QString text = QString::fromUtf8(buffer, result.ptr - buffer);
    if (!text.contains(u'.') && !text.contains(u'e') && !text.contains(u'E')) {
        text.append(u".0");
    }
    return text;
}

/// @brief Returns %g-like text in the shortest representation: "7.5", "10".
/// @param value Number to format.
QString makeShortGeneral(double value)
{
    char buffer[numberBuffer];
    auto result = std::to_chars(buffer, buffer + numberBuffer, value);
    if (result.ec != std::errc()) {
        result = std::to_chars(buffer, buffer + numberBuffer, 0.0);
    }
    return QString::fromUtf8(buffer, result.ptr - buffer);
}

} // namespace

QString pythonFloat(double value)
{
    return makeShortFloat(value);
}

QString generalNumber(double value)
{
    return makeShortGeneral(value);
}

QString formatFrequencyValue(double frequency)
{
    return grouped(qRound64(frequency));
}

QString formatDuration(double seconds)
{
    const auto total = static_cast<qint64>(std::ceil(std::max(seconds, 0.0)));
    return QStringLiteral("%1:%2")
        .arg(QString::number(total / 60),
            QString::number(total % 60).rightJustified(2, u'0'));
}

QString formatDb(double value, bool unit)
{
    // Like %+.2f: the sign is always present, extra zeros of the fraction are removed.
    QString text;
    if (value >= 0.0) {
        text = u'+';
    }
    text += QStringLiteral("%1").arg(std::abs(value), 0, 'f', 2);
    while (text.endsWith(u'0')) {
        text.chop(1);
    }
    if (text.endsWith(u'.')) {
        text.chop(1);
    }
    if (value < 0.0) {
        text.push_front(minusSign);
    }
    return unit ? text + tr("unit.db") : text;
}

QString formatScale(double valueDb)
{
    if (tp::exactEq(valueDb, 0.0)) {
        return QStringLiteral("0");
    }
    QString text = generalNumber(valueDb);
    if (valueDb > 0.0) {
        text.push_front(u'+');
    }
    return text.replace(u'-', minusSign);
}

} // namespace text_format
