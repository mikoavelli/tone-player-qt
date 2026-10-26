// Program entry point.
#include "audio_engine.h"
#include "errors.h"
#include "i18n.h"
#include "models.h"
#include "settings_store.h"
#include "ui/main_window.h"

#include <QApplication>
#include <QDir>
#include <QFont>
#include <QMessageBox>
#include <QStyleFactory>
#include <QTransform>
#include <QtGlobal>

#include <filesystem>
#include <string>
#include <vector>

namespace {

using tp::settings_store::settingsName;


// Directories where settings.json may be located, in order of priority.
std::vector<std::filesystem::path> settingsCandidates(const char *argv0)
{
    std::vector<std::filesystem::path> candidates;
    candidates.push_back(QCoreApplication::applicationDirPath().toStdString());
    if (argv0 != nullptr && *argv0 != 0) {
        candidates.push_back(
            std::filesystem::absolute(std::filesystem::path(argv0)).parent_path().string());
    }
    return candidates;
}

// Directory with settings: the first one where it is actually located.
std::filesystem::path baseDir(const char *argv0)
{
    auto candidates = settingsCandidates(argv0);
    for (const auto &candidate : candidates) {
        if (std::filesystem::exists(candidate / tp::settings_store::settingsName)) {
            return candidate;
        }
    }
    return candidates.front();
}

// Handler of unhandled exceptions: show the detailed text in a dialog.
[[maybe_unused]] void handleException()
{
    const std::exception_ptr current = std::current_exception();
    QString text = tr_ns::tr("app.unexpectedError.text", "unknown");
    if (current != nullptr) {
        try {
            std::rethrow_exception(current);
        } catch (const std::exception &error) {
            text = tr_ns::tr("app.unexpectedError.text", QString::fromUtf8(error.what()));
        } catch (...) {
        }
    }
    QMessageBox::critical(nullptr, tr_ns::tr("app.unexpectedError.title"), text);
}

} // namespace

int main(int argc, char *argv[])
{
    // The starting folder is the one with settings.json: the configuration
    // file is always next to the binary.
    std::filesystem::path settingsPath;
    {
        const auto base = baseDir(argc > 0 ? argv[0] : nullptr);
        std::error_code code;
        std::filesystem::current_path(base, code);
        settingsPath = base / tp::settings_store::settingsName;
    }

    QApplication app(argc, argv);
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    app.setFont(QFont(QStringLiteral("Segoe UI"), 9));

    // The translation is compiled into the binary; initialize it before the
    // settings file is read.
    static_cast<void>(tr_ns::tr("window.title"));

    try {
        const tp::Settings settings =
            tp::settings_store::loadSettingsAtStartup(settingsPath);
        tp::ui::MainWindow window(settings, settingsPath);
        window.show();
        return app.exec();
    } catch (const SettingsNotFoundError &error) {
        QMessageBox::critical(nullptr, tr_ns::tr("startup.settingsNotFound.title"),
            tr_ns::tr("startup.settingsNotFound.text", error.message(),
                QString::fromStdString(std::string{settingsName})));
        return 1;
    } catch (const SettingsError &error) {
        QMessageBox::critical(
            nullptr, tr_ns::tr("startup.settingsError.title"), error.message());
        return 1;
    }
}
