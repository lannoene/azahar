// Copyright 2017-2025 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <algorithm>
#include <atomic>
#include <mutex>
#include <random>
#include <regex>
#include <sstream>
#include <thread>
#include "common/logging/log.h"
#include "common/web_util.h"
#include "netpc/pcall_host.h"
#include "network/room.h"
#include "network/verify_user.h"

namespace Network {

class Room::RoomImpl {
public:
    // This MAC address is used to generate a 'Nintendo' like Mac address.
    const MacAddress NintendoOUI;
    std::mt19937 random_gen; ///< Random number generator. Used for GenerateMacAddress

    std::shared_ptr<PC_Host> server = nullptr; ///< Network interface.

    std::atomic<State> state{State::Closed}; ///< Current state of the room.
    RoomInformation room_information;        ///< Information about this room.

    std::string verify_UID;              ///< A GUID which may be used for verfication.
    mutable std::mutex verify_UID_mutex; ///< Mutex for verify_UID

    std::string password; ///< The password required to connect to this room.

    struct Member {
        std::string nickname;        ///< The nickname of the member.
        std::string console_id_hash; ///< A hash of the console ID of the member.
        GameInfo game_info;          ///< The current game of the member
        MacAddress mac_address;      ///< The assigned mac address of the member.
        /// Data of the user, often including authenticated forum username.
        VerifyUser::UserData user_data;
        std::shared_ptr<PC_Host::Client> peer; ///< The remote peer.
        DeviceType device_type; ///< The device type the user is on.
    };
    using MemberList = std::vector<Member>;
    MemberList members;              ///< Information about the members of this room
    mutable std::mutex member_mutex; ///< Mutex for locking the members list
    /// This should be a std::shared_mutex as soon as C++17 is supported

    UsernameBanList username_ban_list; ///< List of banned usernames
    IPBanList ip_ban_list;             ///< List of banned IP addresses
    mutable std::mutex ban_list_mutex; ///< Mutex for the ban lists

    RoomImpl()
        : NintendoOUI{0x00, 0x1F, 0x32, 0x00, 0x00, 0x00}, random_gen(std::random_device()()) {}

    /// Thread that receives and dispatches network packets
    std::unique_ptr<std::thread> room_thread;

    /// Verification backend of the room
    std::unique_ptr<VerifyUser::Backend> verify_backend;

    /// Thread function that will receive and dispatch messages until the room is destroyed.
    void ServerLoop();
    void StartLoop();

    /**
     * Parses and answers a room join request from a client.
     * Validates the uniqueness of the username and assigns the MAC address
     * that the client will use for the remainder of the connection.
     */
    void HandleJoinRequest(PC_Event* event);

    /**
     * Parses and answers a kick request from a client.
     * Validates the permissions and that the given user exists and then kicks the member.
     */
    void HandleModKickPacket(PC_Event* event);

    /**
     * Parses and answers a ban request from a client.
     * Validates the permissions and bans the user (by forum username or IP).
     */
    void HandleModBanPacket(PC_Event* event);

    /**
     * Parses and answers a unban request from a client.
     * Validates the permissions and unbans the address.
     */
    void HandleModUnbanPacket(PC_Event* event);

    /**
     * Parses and answers a get ban list request from a client.
     * Validates the permissions and returns the ban list.
     */
    void HandleModGetBanListPacket(PC_Event* event);

    /**
     * Returns whether the nickname is valid, ie. isn't already taken by someone else in the room.
     */
    bool IsValidNickname(const std::string& nickname) const;

    /**
     * Returns whether the MAC address is valid, ie. isn't already taken by someone else in the
     * room.
     */
    bool IsValidMacAddress(const MacAddress& address) const;

    /**
     * Returns whether the console ID (hash) is valid, ie. isn't already taken by someone else in
     * the room.
     */
    bool IsValidConsoleId(const std::string& console_id_hash) const;

    /**
     * Returns whether a user has mod permissions.
     */
    bool HasModPermission(std::shared_ptr<BasePeer> client) const;

    /**
     * Sends a ID_ROOM_IS_FULL message telling the client that the room is full.
     */
    void SendRoomIsFull(std::shared_ptr<BasePeer> client);

    /**
     * Sends a ID_ROOM_NAME_COLLISION message telling the client that the name is invalid.
     */
    void SendNameCollision(std::shared_ptr<BasePeer> client);

    /**
     * Sends a ID_ROOM_MAC_COLLISION message telling the client that the MAC is invalid.
     */
    void SendMacCollision(std::shared_ptr<BasePeer> client);

    /**
     * Sends a IdConsoleIdCollison message telling the client that another member with the same
     * console ID exists.
     */
    void SendConsoleIdCollision(std::shared_ptr<BasePeer> client);

    /**
     * Sends a ID_ROOM_VERSION_MISMATCH message telling the client that the version is invalid.
     */
    void SendVersionMismatch(std::shared_ptr<BasePeer> client);

    /**
     * Sends a ID_ROOM_WRONG_PASSWORD message telling the client that the password is wrong.
     */
    void SendWrongPassword(std::shared_ptr<BasePeer> client);

    /**
     * Notifies the member that its connection attempt was successful,
     * and it is now part of the room.
     */
    void SendJoinSuccess(std::shared_ptr<BasePeer> client, MacAddress mac_address);

    /**
     * Notifies the member that its connection attempt was successful,
     * and it is now part of the room, and it has been granted mod permissions.
     */
    void SendJoinSuccessAsMod(std::shared_ptr<BasePeer> client, MacAddress mac_address);

    /**
     * Sends a IdHostKicked message telling the client that they have been kicked.
     */
    void SendUserKicked(std::shared_ptr<BasePeer> client);

    /**
     * Sends a IdHostBanned message telling the client that they have been banned.
     */
    void SendUserBanned(std::shared_ptr<BasePeer> client);

    /**
     * Sends a IdModPermissionDenied message telling the client that they do not have mod
     * permission.
     */
    void SendModPermissionDenied(std::shared_ptr<BasePeer> client);

    /**
     * Sends a IdModNoSuchUser message telling the client that the given user could not be found.
     */
    void SendModNoSuchUser(std::shared_ptr<BasePeer> client);

    /**
     * Sends the ban list in response to a client's request for getting ban list.
     */
    void SendModBanListResponse(std::shared_ptr<BasePeer> client);

    /**
     * Notifies the members that the room is closed,
     */
    void SendCloseMessage();

    /**
     * Sends a system message to all the connected clients.
     */
    void SendStatusMessage(StatusMessageTypes type, const std::string& nickname,
                           const std::string& username, const std::string& ip);

    /**
     * Sends the information about the room, along with the list of members
     * to every connected client in the room.
     * The packet has the structure:
     * <MessageID>ID_ROOM_INFORMATION
     * <String> room_name
     * <String> room_description
     * <u32> member_slots: The max number of clients allowed in this room
     * <String> uid
     * <u16> port
     * <u32> num_members: the number of currently joined clients
     * This is followed by the following three values for each member:
     * <String> nickname of that member
     * <MacAddress> mac_address of that member
     * <String> game_name of that member
     */
    void BroadcastRoomInformation();

    /**
     * Generates a free MAC address to assign to a new client.
     * The first 3 bytes are the NintendoOUI 0x00, 0x1F, 0x32
     */
    MacAddress GenerateMacAddress();

    /**
     * Broadcasts this packet to all members except the sender.
     * @param event The ENet event containing the data
     */
    void HandleWifiPacket(PC_Event* event);

    /**
     * Extracts a chat entry from a received ENet packet and adds it to the chat queue.
     * @param event The ENet event that was received.
     */
    void HandleChatPacket(PC_Event* event);

    /**
     * Extracts the game name from a received ENet packet and broadcasts it.
     * @param event The ENet event that was received.
     */
    void HandleGameNamePacket(PC_Event* event);

    /**
     * Removes the client from the members list if it was in it and announces the change
     * to all other clients.
     */
    void HandleClientDisconnection(std::shared_ptr<BasePeer> client);
};

// RoomImpl
void Room::RoomImpl::ServerLoop() {
    while (state != State::Closed) {
        PC_Event event;
        if (server->Service(event, 16) > 0) {
            switch (event.type) {
            case PC_EV_TYPE_RECEIVE: {
                auto packet_type = reinterpret_cast<const u8*>(event.pk.GetData())[0];
                switch (packet_type) {
                case IdJoinRequest:
                    HandleJoinRequest(&event);
                    break;
                case IdSetGameInfo:
                    HandleGameNamePacket(&event);
                    break;
                case IdWifiPacket:
                    HandleWifiPacket(&event);
                    break;
                case IdChatMessage:
                    HandleChatPacket(&event);
                    break;
                // Moderation
                case IdModKick:
                    HandleModKickPacket(&event);
                    break;
                case IdModBan:
                    HandleModBanPacket(&event);
                    break;
                case IdModUnban:
                    HandleModUnbanPacket(&event);
                    break;
                case IdModGetBanList:
                    HandleModGetBanListPacket(&event);
                    break;
                }
                break;
            }
            case PC_EV_TYPE_DISCONNECT:
                HandleClientDisconnection(event.peer);
                break;
            case PC_EV_TYPE_NONE:
            case PC_EV_TYPE_CONNECT:
                break;
            }
        }
    }
    // Close the connection to all members:
    SendCloseMessage();
}

void Room::RoomImpl::StartLoop() {
    room_thread = std::make_unique<std::thread>(&Room::RoomImpl::ServerLoop, this);
}

void Room::RoomImpl::HandleJoinRequest(PC_Event* event) {
    {
        std::lock_guard lock(member_mutex);
        if (members.size() >= room_information.member_slots) {
            SendRoomIsFull(event->peer);
            return;
        }
    }
    NetPC::Packet& packet = event->pk;
    packet.IgnoreBytes(sizeof(u8)); // Ignore the message type
    std::string nickname;
    packet >> nickname;

    std::string console_id_hash;
    packet >> console_id_hash;

    MacAddress preferred_mac;
    packet >> preferred_mac;

    u32 client_version;
    packet >> client_version;

    std::string pass;
    packet >> pass;

    std::string token;
    packet >> token;

    DeviceType device_type;
    packet.Read(&device_type, sizeof(DeviceType));

    if (pass != password) {
        SendWrongPassword(event->peer);
        return;
    }

    if (!IsValidNickname(nickname)) {
        SendNameCollision(event->peer);
        return;
    }

    if (preferred_mac != NoPreferredMac) {
        // Verify if the preferred mac is available
        if (!IsValidMacAddress(preferred_mac)) {
            SendMacCollision(event->peer);
            return;
        }
    } else {
        // Assign a MAC address of this client automatically
        preferred_mac = GenerateMacAddress();
    }

    if (!IsValidConsoleId(console_id_hash)) {
        SendConsoleIdCollision(event->peer);
        return;
    }

    if (client_version != network_version) {
        SendVersionMismatch(event->peer);
        return;
    }

    // At this point the client is ready to be added to the room.
    Member member{};
    member.mac_address = preferred_mac;
    member.console_id_hash = console_id_hash;
    member.nickname = nickname;
    member.peer = std::static_pointer_cast<PC_Host::Client>(event->peer);
    member.device_type = device_type;

    std::string uid;
    {
        std::lock_guard lock(verify_UID_mutex);
        uid = verify_UID;
    }
    member.user_data = verify_backend->LoadUserData(uid, token);

    if (nickname == room_information.host_username) {
        member.user_data.moderator = true;
        LOG_INFO(Network, "User {} is a moderator", std::string(room_information.host_username));
    }

    std::string ip;
    {
        std::lock_guard lock(ban_list_mutex);

        // Check username ban
        if (!member.user_data.username.empty() &&
            std::find(username_ban_list.begin(), username_ban_list.end(),
                      member.user_data.username) != username_ban_list.end()) {

            SendUserBanned(event->peer);
            return;
        }

        /*// Check IP ban
        char ip_raw[256];
        enet_address_get_host_ip(&event->peer->address, ip_raw, sizeof(ip_raw) - 1);
        ip = ip_raw;

        if (std::find(ip_ban_list.begin(), ip_ban_list.end(), ip) != ip_ban_list.end()) {
            SendUserBanned(event->peer);
            return;
        }*/
    }

    // Notify everyone that the user has joined.
    SendStatusMessage(IdMemberJoin, member.nickname, member.user_data.username, ip);

    {
        std::lock_guard lock(member_mutex);
        members.push_back(std::move(member));
    }

    // Notify everyone that the room information has changed.
    BroadcastRoomInformation();
    if (HasModPermission(event->peer)) {
        SendJoinSuccessAsMod(event->peer, preferred_mac);
    } else {
        SendJoinSuccess(event->peer, preferred_mac);
    }
}

void Room::RoomImpl::HandleModKickPacket(PC_Event *event) {
    if (!HasModPermission(event->peer)) {
        SendModPermissionDenied(event->peer);
        return;
    }

    NetPC::Packet& packet = event->pk;
    packet.IgnoreBytes(sizeof(u8)); // Ignore the message type

    std::string nickname;
    packet >> nickname;

    std::string username, ip;
    {
        std::lock_guard lock(member_mutex);
        const auto target_member =
            std::find_if(members.begin(), members.end(),
                         [&nickname](const auto& member) { return member.nickname == nickname; });
        if (target_member == members.end()) {
            SendModNoSuchUser(event->peer);
            return;
        }

        // Notify the kicked member
        SendUserKicked(target_member->peer);

        username = target_member->user_data.username;

        /*char ip_raw[256];
        enet_address_get_host_ip(&target_member->peer->address, ip_raw, sizeof(ip_raw) - 1);
        ip = ip_raw;

        enet_peer_disconnect(target_member->peer, 0);*/
        members.erase(target_member);
    }

    // Announce the change to all clients.
    SendStatusMessage(IdMemberKicked, nickname, username, ip);
    BroadcastRoomInformation();
}

void Room::RoomImpl::HandleModBanPacket(PC_Event* event) {
    if (!HasModPermission(event->peer)) {
        SendModPermissionDenied(event->peer);
        return;
    }

    NetPC::Packet& packet = event->pk;
    packet.IgnoreBytes(sizeof(u8)); // Ignore the message type

    std::string nickname;
    packet >> nickname;

    std::string username, ip;
    {
        std::lock_guard lock(member_mutex);
        const auto target_member =
            std::find_if(members.begin(), members.end(),
                         [&nickname](const auto& member) { return member.nickname == nickname; });
        if (target_member == members.end()) {
            SendModNoSuchUser(event->peer);
            return;
        }

        // Notify the banned member
        SendUserBanned(target_member->peer);

        nickname = target_member->nickname;
        username = target_member->user_data.username;

        // ip = target_member->peer->GetIpAddress();

        //target_member->peer->Disconnect();
        members.erase(target_member);
    }

    {
        std::lock_guard lock(ban_list_mutex);

        if (!username.empty()) {
            // Ban the forum username
            if (std::find(username_ban_list.begin(), username_ban_list.end(), username) ==
                username_ban_list.end()) {

                username_ban_list.emplace_back(username);
            }
        }

        // Ban the member's IP as well
        if (std::find(ip_ban_list.begin(), ip_ban_list.end(), ip) == ip_ban_list.end()) {
            ip_ban_list.emplace_back(ip);
        }
    }

    // Announce the change to all clients.
    SendStatusMessage(IdMemberBanned, nickname, username, ip);
    BroadcastRoomInformation();
}

void Room::RoomImpl::HandleModUnbanPacket(PC_Event* event) {
    if (!HasModPermission(event->peer)) {
        SendModPermissionDenied(event->peer);
        return;
    }

    NetPC::Packet& packet = event->pk;
    packet.IgnoreBytes(sizeof(u8)); // Ignore the message type

    std::string address;
    packet >> address;

    bool unbanned = false;
    {
        std::lock_guard lock(ban_list_mutex);

        auto it = std::find(username_ban_list.begin(), username_ban_list.end(), address);
        if (it != username_ban_list.end()) {
            unbanned = true;
            username_ban_list.erase(it);
        }

        it = std::find(ip_ban_list.begin(), ip_ban_list.end(), address);
        if (it != ip_ban_list.end()) {
            unbanned = true;
            ip_ban_list.erase(it);
        }
    }

    if (unbanned) {
        SendStatusMessage(IdAddressUnbanned, address, "", "");
    } else {
        SendModNoSuchUser(event->peer);
    }
}

void Room::RoomImpl::HandleModGetBanListPacket(PC_Event* event) {
    if (!HasModPermission(event->peer)) {
        SendModPermissionDenied(event->peer);
        return;
    }

    SendModBanListResponse(event->peer);
}

bool Room::RoomImpl::IsValidNickname(const std::string& nickname) const {
    // A nickname is valid if it matches the regex and is not already taken by anybody else in the
    // room.
    const std::regex nickname_regex("^[ a-zA-Z0-9._-]{4,20}$");
    if (!std::regex_match(nickname, nickname_regex))
        return false;

    std::lock_guard lock(member_mutex);
    return std::all_of(members.begin(), members.end(),
                       [&nickname](const auto& member) { return member.nickname != nickname; });
}

bool Room::RoomImpl::IsValidMacAddress(const MacAddress& address) const {
    // A MAC address is valid if it is not already taken by anybody else in the room.
    std::lock_guard lock(member_mutex);
    return std::all_of(members.begin(), members.end(),
                       [&address](const auto& member) { return member.mac_address != address; });
}

bool Room::RoomImpl::IsValidConsoleId(const std::string& console_id_hash) const {
    // A Console ID is valid if it is not already taken by anybody else in the room.
    std::lock_guard lock(member_mutex);
    return std::all_of(members.begin(), members.end(), [&console_id_hash](const auto& member) {
        return member.console_id_hash != console_id_hash;
    });
}

bool Room::RoomImpl::HasModPermission(std::shared_ptr<BasePeer> client) const {
    std::lock_guard lock(member_mutex);
    const auto sending_member =
        std::find_if(members.begin(), members.end(),
                     [client](const auto& member) { return member.peer == client; });
    if (sending_member == members.end()) {
        return false;
    }
    if (sending_member->user_data.moderator) { // Community moderator

        return true;
    }
    if (!room_information.host_username.empty() &&
        sending_member->user_data.username == room_information.host_username) { // Room host

        return true;
    }
    return false;
}

void Room::RoomImpl::SendNameCollision(std::shared_ptr<BasePeer> client) {
    NetPC::Packet packet;
    packet << static_cast<u8>(IdNameCollision);

    client->SendPacket(packet, PC_SEND_FLAG_RELIABLE);
}

void Room::RoomImpl::SendMacCollision(std::shared_ptr<BasePeer> client) {
    NetPC::Packet packet;
    packet << static_cast<u8>(IdMacCollision);

    client->SendPacket(packet, PC_SEND_FLAG_RELIABLE);
}

void Room::RoomImpl::SendConsoleIdCollision(std::shared_ptr<BasePeer> client) {
    NetPC::Packet packet;
    packet << static_cast<u8>(IdConsoleIdCollision);

    client->SendPacket(packet, PC_SEND_FLAG_RELIABLE);
}

void Room::RoomImpl::SendWrongPassword(std::shared_ptr<BasePeer> client) {
    NetPC::Packet packet;
    packet << static_cast<u8>(IdWrongPassword);

    client->SendPacket(packet, PC_SEND_FLAG_RELIABLE);
}

void Room::RoomImpl::SendRoomIsFull(std::shared_ptr<BasePeer> client) {
    NetPC::Packet packet;
    packet << static_cast<u8>(IdRoomIsFull);

    client->SendPacket(packet, PC_SEND_FLAG_RELIABLE);
}

void Room::RoomImpl::SendVersionMismatch(std::shared_ptr<BasePeer> client) {
    NetPC::Packet packet;
    packet << static_cast<u8>(IdVersionMismatch);
    packet << network_version;

    client->SendPacket(packet, PC_SEND_FLAG_RELIABLE);
}

void Room::RoomImpl::SendJoinSuccess(std::shared_ptr<BasePeer> client, MacAddress mac_address) {
    NetPC::Packet packet;
    packet << static_cast<u8>(IdJoinSuccess);
    packet << mac_address;

    client->SendPacket(packet, PC_SEND_FLAG_RELIABLE);
}

void Room::RoomImpl::SendJoinSuccessAsMod(std::shared_ptr<BasePeer> client, MacAddress mac_address) {
    NetPC::Packet packet;
    packet << static_cast<u8>(IdJoinSuccessAsMod);
    packet << mac_address;

    client->SendPacket(packet, PC_SEND_FLAG_RELIABLE);
}

void Room::RoomImpl::SendUserKicked(std::shared_ptr<BasePeer> client) {
    NetPC::Packet packet;
    packet << static_cast<u8>(IdHostKicked);

    client->SendPacket(packet, PC_SEND_FLAG_RELIABLE);
}

void Room::RoomImpl::SendUserBanned(std::shared_ptr<BasePeer> client) {
    NetPC::Packet packet;
    packet << static_cast<u8>(IdHostBanned);

    client->SendPacket(packet, PC_SEND_FLAG_RELIABLE);
}

void Room::RoomImpl::SendModPermissionDenied(std::shared_ptr<BasePeer> client) {
    NetPC::Packet packet;
    packet << static_cast<u8>(IdModPermissionDenied);

    client->SendPacket(packet, PC_SEND_FLAG_RELIABLE);
}

void Room::RoomImpl::SendModNoSuchUser(std::shared_ptr<BasePeer> client) {
    NetPC::Packet packet;
    packet << static_cast<u8>(IdModNoSuchUser);

    client->SendPacket(packet, PC_SEND_FLAG_RELIABLE);
}

void Room::RoomImpl::SendModBanListResponse(std::shared_ptr<BasePeer> client) {
    NetPC::Packet packet;
    packet << static_cast<u8>(IdModBanListResponse);
    {
        std::lock_guard lock(ban_list_mutex);
        packet << username_ban_list;
        packet << ip_ban_list;
    }

    client->SendPacket(packet, PC_SEND_FLAG_RELIABLE);
}

void Room::RoomImpl::SendCloseMessage() {
    NetPC::Packet packet;
    packet << static_cast<u8>(IdCloseRoom);
    std::lock_guard lock(member_mutex);
    if (!members.empty()) {
        for (auto& member : members) {
            member.peer->SendPacket(packet, PC_SEND_FLAG_RELIABLE);
        }
    }
    for (auto& member : members) {
        //member.peer->Disconnect();
    }
}

void Room::RoomImpl::SendStatusMessage(StatusMessageTypes type, const std::string& nickname,
                                       const std::string& username, const std::string& ip) {
    NetPC::Packet packet;
    packet << static_cast<u8>(IdStatusMessage);
    packet << static_cast<u8>(type);
    packet << nickname;
    packet << username;
    {
        std::lock_guard lock(member_mutex);
        for (auto& member : members) {
            member.peer->SendPacket(packet, PC_SEND_FLAG_RELIABLE);
        }
    }

    const std::string display_name =
        username.empty() ? nickname : fmt::format("{} ({})", nickname, username);

    switch (type) {
    case IdMemberJoin:
        LOG_INFO(Network, "[{}] {} has joined.", ip, display_name);
        break;
    case IdMemberLeave:
        LOG_INFO(Network, "[{}] {} has left.", ip, display_name);
        break;
    case IdMemberKicked:
        LOG_INFO(Network, "[{}] {} has been kicked.", ip, display_name);
        break;
    case IdMemberBanned:
        LOG_INFO(Network, "[{}] {} has been banned.", ip, display_name);
        break;
    case IdAddressUnbanned:
        LOG_INFO(Network, "{} has been unbanned.", display_name);
        break;
    }
}

void Room::RoomImpl::BroadcastRoomInformation() {
    NetPC::Packet packet;
    packet << static_cast<u8>(IdRoomInformation);
    packet << room_information.name;
    packet << room_information.description;
    packet << room_information.member_slots;
    packet << room_information.port;
    packet << room_information.preferred_game;
    packet << room_information.host_username;

    packet << static_cast<u32>(members.size());
    {
        std::lock_guard lock(member_mutex);
        for (const auto& member : members) {
            packet << member.nickname;
            packet << member.mac_address;
            packet << member.game_info.name;
            packet << member.game_info.id;
            packet << member.user_data.username;
            packet << member.user_data.display_name;
            packet << member.user_data.avatar_url;
            packet << static_cast<u8>(member.device_type);
        }
    }

    server->BroadcastPacket(packet, PC_SEND_FLAG_RELIABLE, nullptr);
}

MacAddress Room::RoomImpl::GenerateMacAddress() {
    MacAddress result_mac =
        NintendoOUI; // The first three bytes of each MAC address will be the NintendoOUI
    std::uniform_int_distribution<> dis(0x00, 0xFF); // Random byte between 0 and 0xFF
    do {
        for (std::size_t i = 3; i < result_mac.size(); ++i) {
            result_mac[i] = dis(random_gen);
        }
    } while (!IsValidMacAddress(result_mac));
    return result_mac;
}

void Room::RoomImpl::HandleWifiPacket(PC_Event* event) {
    NetPC::Packet& in_packet = event->pk;
    in_packet.IgnoreBytes(sizeof(u8));         // Message type
    in_packet.IgnoreBytes(sizeof(u8));         // WifiPacket Type
    in_packet.IgnoreBytes(sizeof(u8));         // WifiPacket Channel
    in_packet.IgnoreBytes(sizeof(MacAddress)); // WifiPacket Transmitter Address
    MacAddress destination_address;
    in_packet >> destination_address;

    if (destination_address == BroadcastMac) { // Send the data to everyone except the sender
        std::lock_guard lock(member_mutex);

        server->BroadcastPacket(in_packet, event->pk_flags, std::static_pointer_cast<PC_Host::Client>(event->peer));
    } else { // Send the data only to the destination client
        std::lock_guard lock(member_mutex);
        auto member = std::find_if(members.begin(), members.end(),
                                   [destination_address](const Member& member) -> bool {
                                       return member.mac_address == destination_address;
                                   });
        if (member != members.end()) {
            member->peer->SendPacket(in_packet, event->pk_flags);
        } else {
            LOG_ERROR(Network,
                      "Attempting to send to unknown MAC address: {}",
                      Common::MacToString(destination_address));
        }
    }
}

void Room::RoomImpl::HandleChatPacket(PC_Event* event) {
    NetPC::Packet& in_packet = event->pk;

    in_packet.IgnoreBytes(sizeof(u8)); // Ignore the message type
    std::string message;
    in_packet >> message;
    auto CompareNetworkAddress = [event](const Member member) -> bool {
        return member.peer == event->peer;
    };

    std::lock_guard lock(member_mutex);
    const auto sending_member = std::find_if(members.begin(), members.end(), CompareNetworkAddress);
    if (sending_member == members.end()) {
        return; // Received a chat message from a unknown sender
    }

    // Limit the size of chat messages to MaxMessageSize
    message.resize(std::min(static_cast<u32>(message.size()), MaxMessageSize));

    NetPC::Packet out_packet;
    out_packet << static_cast<u8>(IdChatMessage);
    out_packet << sending_member->nickname;
    out_packet << sending_member->user_data.username;
    out_packet << message;

    for (const auto& member : members) {
        if (member.peer != event->peer) {
            member.peer->SendPacket(out_packet, PC_SEND_FLAG_RELIABLE);
        }
    }

    if (sending_member->user_data.username.empty()) {
        LOG_INFO(Network, "{}: {}", sending_member->nickname, message);
    } else {
        LOG_INFO(Network, "{} ({}): {}", sending_member->nickname,
                 sending_member->user_data.username, message);
    }
}

void Room::RoomImpl::HandleGameNamePacket(PC_Event* event) {
    NetPC::Packet in_packet = event->pk;

    in_packet.IgnoreBytes(sizeof(u8)); // Ignore the message type
    GameInfo game_info;
    in_packet >> game_info.name;
    in_packet >> game_info.id;

    {
        std::lock_guard lock(member_mutex);
        auto member =
            std::find_if(members.begin(), members.end(), [event](const Member& member) -> bool {
                return member.peer == event->peer;
            });
        if (member != members.end()) {
            member->game_info = game_info;

            const std::string display_name =
                member->user_data.username.empty()
                    ? member->nickname
                    : fmt::format("{} ({})", member->nickname, member->user_data.username);

            if (game_info.name.empty()) {
                LOG_INFO(Network, "{} is not playing", display_name);
            } else {
                LOG_INFO(Network, "{} is playing {}", display_name, game_info.name);
            }
        }
    }
    BroadcastRoomInformation();
}

void Room::RoomImpl::HandleClientDisconnection(std::shared_ptr<BasePeer> client) {
    // Remove the client from the members list.
    std::string nickname, username, ip;
    {
        std::lock_guard lock(member_mutex);
        auto member = std::find_if(members.begin(), members.end(), [client](const Member& member) {
            return member.peer == client;
        });
        if (member != members.end()) {
            nickname = member->nickname;
            username = member->user_data.username;

            /*char ip_raw[256];
            enet_address_get_host_ip(&member->peer->address, ip_raw, sizeof(ip_raw) - 1);
            ip = ip_raw;*/

            members.erase(member);
        }
    }

    // Announce the change to all clients.
    //client->Disconnect();
    if (!nickname.empty())
        SendStatusMessage(IdMemberLeave, nickname, username, ip);
    BroadcastRoomInformation();
}

// Room
Room::Room() : room_impl{std::make_unique<RoomImpl>()} {}

Room::~Room() = default;

bool Room::Create(const std::string& name, const std::string& description,
                  const std::string& server_address, u16 server_port, const std::string& password,
                  const u32 max_connections, const std::string& host_username,
                  const std::string& preferred_game, u64 preferred_game_id,
                  std::unique_ptr<VerifyUser::Backend> verify_backend,
                  const Room::BanList& ban_list) {
    // In order to send the room is full message to the connecting client, we need to leave one
    // slot open so enet won't reject the incoming connection without telling us
    room_impl->server = std::make_shared<PC_Host>();
    //room_impl->server->SetMaxPeers(max_connections + 1);
    if (!server_address.empty()) {
        //room_impl->server->SetIpAddress(server_address);
    }
    if (!room_impl->server) {
        return false;
    }
    room_impl->server->Init(server_port);
    room_impl->state = State::Open;

    room_impl->room_information.name = name;
    room_impl->room_information.description = description;
    room_impl->room_information.member_slots = max_connections;
    room_impl->room_information.port = server_port;
    room_impl->room_information.preferred_game = preferred_game;
    room_impl->room_information.preferred_game_id = preferred_game_id;
    room_impl->room_information.host_username = host_username;
    room_impl->password = password;
    room_impl->verify_backend = std::move(verify_backend);
    room_impl->username_ban_list = ban_list.first;
    room_impl->ip_ban_list = ban_list.second;

    room_impl->StartLoop();
    return true;
}

Room::State Room::GetState() const {
    return room_impl->state;
}

const RoomInformation& Room::GetRoomInformation() const {
    return room_impl->room_information;
}

std::string Room::GetVerifyUID() const {
    std::lock_guard lock(room_impl->verify_UID_mutex);
    return room_impl->verify_UID;
}

Room::BanList Room::GetBanList() const {
    std::lock_guard lock(room_impl->ban_list_mutex);
    return {room_impl->username_ban_list, room_impl->ip_ban_list};
}

std::vector<MemberInformation> Room::GetRoomMemberList() const {
    std::vector<MemberInformation> member_list;
    std::lock_guard lock(room_impl->member_mutex);
    for (const auto& member_impl : room_impl->members) {
        MemberInformation member;
        member.nickname = member_impl.nickname;
        member.username = member_impl.user_data.username;
        member.display_name = member_impl.user_data.display_name;
        member.avatar_url = member_impl.user_data.avatar_url;
        member.mac_address = member_impl.mac_address;
        member.game_info = member_impl.game_info;
        member.device_type = member_impl.device_type;
        member_list.push_back(member);
    }
    return member_list;
}

bool Room::HasPassword() const {
    return !room_impl->password.empty();
}

void Room::SetVerifyUID(const std::string& uid) {
    std::lock_guard lock(room_impl->verify_UID_mutex);
    room_impl->verify_UID = uid;
}

void Room::Destroy() {
    room_impl->state = State::Closed;
    room_impl->room_thread->join();
    room_impl->room_thread.reset();

    if (room_impl->server) {
        room_impl->server.reset();
    }
    room_impl->room_information = {};
    room_impl->server = nullptr;
    {
        std::lock_guard lock(room_impl->member_mutex);
        room_impl->members.clear();
    }
    room_impl->room_information.member_slots = 0;
    room_impl->room_information.name.clear();
}

} // namespace Network
