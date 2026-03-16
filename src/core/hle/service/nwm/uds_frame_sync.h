#pragma once

#include "network/network.h"
#include "core/core.h"

namespace Service::NWM {

class NWM_UDS;

// An NWM_UDS net_frame_sync interface
// with the GPU's net_frame_sync class.

// Can be either accessed by the GPU class
// or by UDS itself

class UDSNetFrameSyncImpl {
public:
    UDSNetFrameSyncImpl(NWM_UDS& _uds);
    void SendFrameReadyPacketAndWait(u64 ms_how_long = -1);
    void HandleNetFrameSyncPacket(const Network::WifiPacket& packet);
    void CheckAdvanceNextFrame();
    bool IsEnabled();
    void NotifyDisabled(); // since our thread may be waiting, we can tell it it's disabled
private:
    Core::System& system;
    NWM_UDS& uds;
    
    // shared
    std::mutex thread_notify_mutex;
    std::condition_variable cv_thread_notify;
    bool can_advance_next_frame; // protected by thread_notify_mutex
    std::atomic<u64> current_frame{}; // TODO: keep track of which frame we're on
    std::atomic<u64> num_frames_advance{}; // decr each frame until 0 then wait
    int frame_skip; // sends sync packet every x frames
    
    // client only
    bool got_current_frame_yet;
    
    // host only
    std::recursive_mutex node_ready_map_mutex;
    
    struct ClientFrameInfo {
        
    };
    std::unordered_map<u16, bool> node_frame_ready_map; // protected by node_ready_map_mutex
    
    
    bool IsHost();
    bool IsConnected();
    
    // on receive
    void ClientHandleFrameSyncPacket(const Network::WifiPacket& packet);
    void ServerHandleFrameSyncPacket(const Network::WifiPacket& packet);
    
    // on send
    void ClientSendFrameSyncPacket();
    void ServerSendFrameSyncPacket();
    
    bool AllPeersReadyToAdvance();
    void PrintWaitDebugInfo();
};

} // Service::NWM
