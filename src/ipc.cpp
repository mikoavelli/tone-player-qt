/// @file ipc.cpp
/// @brief Implementation of the named pipe remote control server.
#include "ipc.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>

namespace tp {
namespace ipc {

namespace {

/// @brief Reads one request line, dispatches it and answers in JSON.
/// Connections are cheap; no caching between requests.
void serveConnection(QLocalSocket *socket, const CallHandler &handler)
{
    QObject::connect(socket, &QLocalSocket::readyRead, socket, [socket, handler]() {
        QByteArray buffer = socket->property("ipcBuffer").toByteArray();
        if (buffer.size() > maxRequestBytes) {
            socket->abort();
            return;
        }
        buffer += socket->readAll();
        const qsizetype newline = buffer.indexOf('\n');
        if (newline < 0) {
            if (buffer.size() > maxRequestBytes) {
                socket->abort();
                return;
            }
            socket->setProperty("ipcBuffer", buffer);
            return; // partial line — wait for more
        }
        QByteArray line = buffer.left(newline);
        buffer.clear(); // consumed; nothing left for the next request
        socket->setProperty("ipcBuffer", QByteArray());
        line.replace("\r", "");
        const QList<QByteArray> parts = line.split(' ');
        QStringList args;
        for (const QByteArray &part : parts) {
            const QString trimmed = QString::fromUtf8(part).trimmed();
            if (!trimmed.isEmpty()) {
                args.append(trimmed);
            }
        }
        QString message;
        bool ok = false;
        if (args.isEmpty()) {
            message = QStringLiteral("empty request");
        } else {
            const QString name = args.takeFirst();
            try {
                message = handler(name, args);
                ok = true;
            } catch (const std::exception &error) {
                message = QString::fromUtf8(error.what());
            } catch (...) {
                message = QStringLiteral("internal error");
            }
        }
        QJsonObject respond;
        respond.insert(QStringLiteral("ok"), ok);
        respond.insert(QStringLiteral("message"), message);
        const QByteArray out = QJsonDocument(respond).toJson(QJsonDocument::Compact) + '\n';
        socket->write(out);
        socket->disconnectFromServer();
    });
    QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
}

} // namespace

IpcServer::IpcServer(CallHandler handler, QObject *parent)
    : QObject(parent)
    , handler_(std::move(handler))
{
}

bool IpcServer::listen()
{
    // Another instance may have died leaving a stale pipe entry; the Python
    // version steals it as well.
    QLocalServer::removeServer(QString::fromLatin1(serverName));
    server_ = new QLocalServer(this);
    connect(server_, &QLocalServer::newConnection, this, &IpcServer::onNewConnection);
    return server_->listen(QString::fromLatin1(serverName));
}

void IpcServer::onNewConnection()
{
    if (server_ == nullptr) {
        return;
    }
    while (server_->hasPendingConnections()) {
        QLocalSocket *socket = server_->nextPendingConnection();
        if (socket != nullptr) {
            serveConnection(socket, handler_);
        }
    }
}

} // namespace ipc
} // namespace tp
