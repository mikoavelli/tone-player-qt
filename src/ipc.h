/// @file ipc.h
/// @brief Minimal IPC over a named pipe (QLocalServer) for remote control.
///
/// The running program keeps a QLocalServer with a fixed name (on Windows:
/// `\\.\\pipe\\tone-player`). A client connects, sends one line
/// `name [argument...]`, and receives one line of JSON `{"ok": ..., "message": ...}`
/// — the same wire format as the Python version of the app.
///
/// Supported calls:
///   status            one-line player summary
///   play [index]      start or resume playback (zero-based band index)
///   stop              stop playback and reset to the start
///   toggle            playing/paused -> stop; otherwise start from the focus
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

class QLocalServer;
class QLocalSocket;

namespace tp::ipc {

inline constexpr auto serverName = "tone-player";
/// @brief Longest accepted request, bytes; anything above is rejected.
inline constexpr qsizetype maxRequestBytes = 4096;

/// @brief Answers a remote call; throws CallError-compatible failure text on error.
using CallHandler = std::function<QString(const QString &name, const QStringList &args)>;

/// @brief Listening server; one request at a time, answered in one JSON line.
class IpcServer final : public QObject {
    Q_OBJECT

public:
    /// @brief Creates the server. The caller must invoke @ref listen().
    explicit IpcServer(CallHandler handler, QObject *parent = nullptr);

    /// @brief Starts listening, stealing a stale pipe left by a crashed instance.
    /// @return false when another live instance already holds the name.
    [[nodiscard]] bool listen();

private slots:
    void onNewConnection();

private:
    QLocalServer *server_ = nullptr;
    CallHandler handler_;
};

} // namespace tp::ipc
