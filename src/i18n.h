// Source strings and their translation into the language compiled into the binary.
// en.json lives in a Qt resource with the path TONE_PLAYER_TRANSLATION
// (set in CMakeLists.txt through the TONE_PLAYER_LANGUAGE option),
// therefore unit and the settings file are independent of external files.
#pragma once

#include <QString>
#include <string>
#include <type_traits>

namespace tr_ns {

// Text by key. A missing key falls back to the key itself, so that a missing
// line does not turn into text in another language.
QString tr(const char *key);

namespace detail {

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

// Substitution %1..%n into the text of the key, from left to right.
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
