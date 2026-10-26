/// @file i18n.h
/// @brief Source strings and their translation into the language compiled into the binary.
///
/// The translation json lives in a Qt resource with the path
/// TONE_PLAYER_TRANSLATION (set in CMakeLists.txt through the
/// TONE_PLAYER_LANGUAGE option), therefore the interface and the settings file
/// are independent of external files.
#pragma once

#include <QString>
#include <string>
#include <type_traits>

/// @brief Runtime translation of interface strings.
namespace tr_ns {

/// @brief Returns the text by key.
///
/// A missing key falls back to the key itself, so that a missing line does not
/// turn into text in another language.
/// @param key Translation key from the compiled json resource.
QString tr(const char *key);

/// @brief Helpers for @ref tr.
namespace detail {

/// @brief Converts one substitution argument to text.
/// @param value QString, UTF-8 string or a number.
/// @return Text representation of the argument.
template <typename T>
inline QString toStr(const T &value)
{
    if constexpr (std::is_same_v<std::decay_t<T>, QString>) {
        return value;
    } else if constexpr (std::is_convertible_v<T, const char *>) {
        return QString::fromUtf8(value);
    } else {
        return QString::number(value);
    }
}

} // namespace detail

/// @brief Substitutes @c %1..%n into the text of the key, from left to right.
/// @param key Translation key.
/// @param args Values for the placeholders, in order.
/// @return Translated text with the placeholders replaced.
template <typename... Args>
QString tr(const char *key, const Args &...args)
{
    QString parts[] = { detail::toStr(args)... };
    QString result = tr(key);
    int index = static_cast<int>(sizeof...(args));
    while (index >= 1) {
        result.replace(u'%' + QString::number(index), parts[index - 1]);
        --index;
    }
    return result;
}

} // namespace tr_ns
