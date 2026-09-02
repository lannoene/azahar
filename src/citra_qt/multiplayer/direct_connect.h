// Copyright 2017-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <mutex>
#include <QDialog>
#include <QFutureWatcher>
#include "citra_qt/multiplayer/validation.h"
#include "network/room_member.h"

namespace Ui {
class DirectConnect;
}

namespace Core {
class System;
}

class DirectConnectWindow : public QDialog {
    Q_OBJECT

public:
    explicit DirectConnectWindow(Core::System& system, QWidget* parent = nullptr);
    ~DirectConnectWindow();

    void RetranslateUi();

protected:
    void showEvent(QShowEvent* event) override;

signals:
    /**
     * Signalled by this widget when it is closing itself and destroying any state such as
     * connections that it might have.
     */
    void Closed();

private slots:
    void OnConnection();

private:
    void Connect();
    void BeginConnecting();
    void EndConnecting();
    void OnStateChange(const Network::RoomMember::State& state);
    void OnConnectionError(const Network::RoomMember::Error& error);

    QFutureWatcher<void>* watcher;
    std::unique_ptr<Ui::DirectConnect> ui;
    Core::System& system;
    Validation validation;
    QFuture<void> connect_future;
    std::mutex connection_lock;
    std::condition_variable cv_connect;
};
