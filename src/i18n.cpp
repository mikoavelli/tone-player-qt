/// @file i18n.cpp
/// @brief Loading of interface strings from the resource compiled into the binary.
///
/// Implementation of the lookup declared in i18n.h.
#include "i18n.h"

#include <QCoreApplication>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLatin1String>
#include <QResource>

#ifdef TONE_PLAYER_TRANSLATION
#define TP_TRANSLATION_RESOURCE QStringLiteral(TONE_PLAYER_TRANSLATION)
#else
#error "TONE_PLAYER_TRANSLATION is not defined by the build system"
#endif

namespace tr_ns {

namespace {

/// @brief Returns the translation table, loading the json resource on first use.
QHash<QString, QString> &table()
{
    static QHash<QString, QString> cached = [] {
        QHash<QString, QString> map;
        QFile file(TP_TRANSLATION_RESOURCE);
        if (!file.open(QIODeviceBase::ReadOnly)) {
            qWarning("Translation resource %s is missing.", qPrintable(TP_TRANSLATION_RESOURCE));
            return map;
        }
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
        if (!document.isObject()) {
            qWarning("Translation resource %s must contain a flat json object.",
                     qPrintable(TP_TRANSLATION_RESOURCE));
            return map;
        }
        const QJsonObject root = document.object();
        for (auto it = root.begin(); it != root.end(); ++it) {
            if (it.value().isString()) {
                map.insert(it.key(), it.value().toString());
            }
        }
        return map;
    }();
    return cached;
}

} // namespace

QString tr(const char *key)
{
    const auto it = table().find(QLatin1String(key));
    return it == table().end() ? QString::fromLatin1(key) : it.value();
}

} // namespace tr_ns
