#pragma once

#include "core/core.h"

namespace Service::NWM {
class NWM_UDS;
} // Service::NWM

namespace Network {

class NetFrameSynchronizer {
public:
    NetFrameSynchronizer();
    // must be enabled by the
    // UDS host in settings &
    // we are connected
    bool IsEnabled();
    void WaitSyncNextFrame();
private:
    std::shared_ptr<Service::NWM::NWM_UDS> GetUDS();

    Core::System& system;
    
    u64 ms_frame_wait;
};

}; // namespace Network