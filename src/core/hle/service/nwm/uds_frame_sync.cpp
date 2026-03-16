#include "core/hle/service/nwm/uds_frame_sync.h"
#include "core/hle/service/nwm/nwm_uds.h"
#include "network/frame_sync.h"
#include "video_core/gpu.h"
#include "common/timer.h"


namespace Service::NWM {

UDSNetFrameSyncImpl::UDSNetFrameSyncImpl(NWM_UDS& _uds) : uds(_uds), system(Core::System::GetInstance()), frame_skip(60), got_current_frame_yet(false) { }

struct NetFrameSyncPacket {
    u64 current_frame;
    // set by host only
    s64 advance_frames; // pos: run for x frames long, neg: stop
};

void UDSNetFrameSyncImpl::HandleNetFrameSyncPacket(const Network::WifiPacket& packet) {
    //LOG_INFO(Service_NWM, "Called");
    if (!IsHost()) {
        ClientHandleFrameSyncPacket(packet);
    } else {
        ServerHandleFrameSyncPacket(packet);
    }
    //LOG_INFO(Service_NWM, "Finished");
}

void UDSNetFrameSyncImpl::ClientHandleFrameSyncPacket(const Network::WifiPacket& packet) {
    auto& gpu_frame_sync = system.GPU().GetNetFrameSynchronizer();
    
    auto payload = reinterpret_cast<const NetFrameSyncPacket*>(packet.data.data());
    
    if (!got_current_frame_yet) {
        current_frame = payload->current_frame;
        got_current_frame_yet = true;
    }
    
    {
        std::unique_lock lock(thread_notify_mutex);
        if (can_advance_next_frame) {
            LOG_WARNING(Service_NWM, "Got advance next frame command when we were already going to advance");
        }
        can_advance_next_frame = true;
    }
    cv_thread_notify.notify_one();
}

void UDSNetFrameSyncImpl::ServerHandleFrameSyncPacket(const Network::WifiPacket& packet) {
    auto& gpu_frame_sync = system.GPU().GetNetFrameSynchronizer();
    
    auto payload = reinterpret_cast<const NetFrameSyncPacket*>(packet.data.data());
    
    {
        std::scoped_lock lock{thread_notify_mutex, node_ready_map_mutex, uds.connection_status_mutex};
        
        // find node_id of client
        auto node_it = uds.node_map.find(packet.transmitter_address);
        if (node_it == uds.node_map.end()) {
            LOG_ERROR(Service_NWM, "Client is not connected");
            return;
        }
        auto& node = node_it->second;
        if (node.spec || !node.connected) {
            return; // ignore
        }
        // make sure node id makes sense
        if (!node.node_id || node.node_id > UDSMaxNodes) {
            LOG_ERROR(Service_NWM, "Got bad node id {}", node.node_id);
            return;
        }
        s64 frame_desync = static_cast<s64>(payload->current_frame) - static_cast<s64>(current_frame);
        LOG_INFO(Service_NWM, "Client frame desync: {}", frame_desync);
        node_frame_ready_map[node.node_id] = true;
        can_advance_next_frame = AllPeersReadyToAdvance();
    }
    cv_thread_notify.notify_one();
}

void UDSNetFrameSyncImpl::CheckAdvanceNextFrame() {
    {
        std::scoped_lock lock{thread_notify_mutex};
        can_advance_next_frame = AllPeersReadyToAdvance();
    }
    cv_thread_notify.notify_one();
}

void UDSNetFrameSyncImpl::NotifyDisabled() {
    //LOG_INFO(Service_NWM, "Notifying disabled");
    {
        std::scoped_lock lock{thread_notify_mutex};
        can_advance_next_frame = true;
        got_current_frame_yet = false;
    }
    cv_thread_notify.notify_one();
    //LOG_INFO(Service_NWM, "Finished notifying disabled");
}

bool UDSNetFrameSyncImpl::IsHost() {
    auto conn_status = uds.GetConnectionStatusHLE(false);
    return conn_status.status == NWM::NetworkStatus::ConnectedAsHost;
}

bool UDSNetFrameSyncImpl::IsEnabled() {
    return IsConnected();
}

bool UDSNetFrameSyncImpl::IsConnected() {
    auto conn_status = uds.GetConnectionStatusHLE(false);
    return conn_status.status == NWM::NetworkStatus::ConnectedAsHost ||
           conn_status.status == NWM::NetworkStatus::ConnectedAsClient;
}

void UDSNetFrameSyncImpl::SendFrameReadyPacketAndWait(u64 ms_how_long) {
    //LOG_INFO(Service_NWM, "Called");
    constexpr bool use_debug_wait = true;
    u64 debug_wait_issue_ms = 5000; // ms to wait until we print error info
    Common::Timer debug_wait_timer;
    std::unique_lock lock(thread_notify_mutex);
    if (current_frame % frame_skip == 0) {
        if (!IsHost()) {
            /*if (num_frames_advance) {
                num_frames_advance--;
                return;
            }*/
            // tell the server we're ready
            ClientSendFrameSyncPacket();
        } else {
            std::scoped_lock cs_lock{uds.connection_status_mutex, node_ready_map_mutex};
            auto conn_status = uds.GetConnectionStatusHLE(false);
            if (conn_status.total_nodes <= 1) {
                return;
            }
            node_frame_ready_map[HostDestNodeId] = true;
            can_advance_next_frame = AllPeersReadyToAdvance();
        }
        if (!can_advance_next_frame) {
            // wait until condition_variable is signaled
            if (ms_how_long != -1) {
                cv_thread_notify.wait_for(lock, std::chrono::milliseconds(ms_how_long), [this]() { return can_advance_next_frame; });
            } else {
                if (!use_debug_wait) {
                    cv_thread_notify.wait(lock, [this]() { return can_advance_next_frame; });
                } else {
                    debug_wait_timer.Start();
                    cv_thread_notify.wait_for(lock, std::chrono::milliseconds(debug_wait_issue_ms), [&]() {
                        if (!can_advance_next_frame && debug_wait_timer.GetTimeDifference().count() >= debug_wait_issue_ms) {
                            PrintWaitDebugInfo();
                        }
                        return can_advance_next_frame;
                    });
                }
            }
        }
        if (IsHost()) {
            // tell the clients we're ready
            ServerSendFrameSyncPacket();
            std::scoped_lock map_lock(node_ready_map_mutex);
            node_frame_ready_map.clear();
        } else {
            can_advance_next_frame = false;
        }
    }
    //LOG_INFO(Service_NWM, "Finished");
    current_frame++;
}

void UDSNetFrameSyncImpl::ClientSendFrameSyncPacket() {
    Network::WifiPacket packet;
    packet.type = Network::WifiPacket::PacketType::FrameSync;
    packet.destination_address = uds.network_info.host_mac_address;
    NetFrameSyncPacket payload;
    payload.current_frame = current_frame;
    
    std::span<u8> payload_buf{reinterpret_cast<u8*>(&payload), sizeof(NetFrameSyncPacket)};
    packet.data = {payload_buf.begin(), payload_buf.end()};
    SendPacket(packet);
}

void UDSNetFrameSyncImpl::ServerSendFrameSyncPacket() {
    Network::WifiPacket packet;
    packet.type = Network::WifiPacket::PacketType::FrameSync;
    packet.destination_address = Network::BroadcastMac;
    NetFrameSyncPacket payload;
    payload.current_frame = current_frame;
    
    std::span<u8> payload_buf{reinterpret_cast<u8*>(&payload), sizeof(NetFrameSyncPacket)};
    packet.data = {payload_buf.begin(), payload_buf.end()};
    SendPacket(packet);
}

bool UDSNetFrameSyncImpl::AllPeersReadyToAdvance() {
    if (!IsHost()) { // if we are a client, we must wait
        return false;
    }
    std::scoped_lock lock{uds.connection_status_mutex, node_ready_map_mutex};
    auto conn_status = uds.GetConnectionStatusHLE(false);
    for (u16 node_index = 0; node_index < UDSMaxNodes; node_index++) {
        auto node_id = conn_status.nodes[node_index];
        if (node_id && !node_frame_ready_map[node_id]) {
            return false;
        }
    }
    return true;
}

void UDSNetFrameSyncImpl::PrintWaitDebugInfo() {
    LOG_ERROR(Service_NWM, "It seems we hit a snag. Error info: Is host {}, current_frame {}, frame_skip {}", IsHost(), static_cast<u64>(current_frame), frame_skip);
    if (!IsHost()) {
        LOG_ERROR(Service_NWM, "Client info: got_current_frame_yet {}", got_current_frame_yet);
    } else {
        LOG_ERROR(Service_NWM, "Server info: AllPeersReadyToAdvance() {}", AllPeersReadyToAdvance());
    }
}

} // Service::NWM