// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include "common/assert.h"
#include "common/logging/log.h"
#include "netpc/pcall_host.h"
#include "network/network.h"

namespace Network {

static std::shared_ptr<RoomMember> g_room_member; ///< RoomMember (Client) for network games
static std::shared_ptr<Room> g_room;              ///< Room (Server) for network games
// TODO(B3N30): Put these globals into a networking class

bool Init() {
    BaseSocket::InitNetworking();
    g_room = std::make_shared<Room>();
    g_room_member = std::make_shared<RoomMember>();
    LOG_DEBUG(Network, "initialized OK");
    return true;
}

std::weak_ptr<Room> GetRoom() {
    return g_room;
}

std::weak_ptr<RoomMember> GetRoomMember() {
    return g_room_member;
}

void Shutdown() {
    if (g_room_member) {
        if (g_room_member->IsConnected())
            g_room_member->Leave();
        g_room_member.reset();
    }
    if (g_room) {
        if (g_room->GetState() == Room::State::Open)
            g_room->Destroy();
        g_room.reset();
    }
    BaseSocket::DeinitNetworking();
    LOG_DEBUG(Network, "shutdown OK");
}

DeviceType GetCurrentDeviceType() {
#ifdef ENABLE_QT
    return DeviceType::Computer;
#endif
#ifdef ANDROID
    return DeviceType::Phone;
#endif
    return DeviceType::Unknown;
}

} // namespace Network
