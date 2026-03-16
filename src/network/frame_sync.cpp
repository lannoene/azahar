#include "network/frame_sync.h"

#include "core/hle/service/nwm/nwm_uds.h"

namespace Network {

std::shared_ptr<Service::NWM::NWM_UDS> NetFrameSynchronizer::GetUDS() {
    return system.ServiceManager().GetService<Service::NWM::NWM_UDS>("nwm::UDS");
}

NetFrameSynchronizer::NetFrameSynchronizer() : system(Core::System::GetInstance()), ms_frame_wait(500) { }

void NetFrameSynchronizer::WaitSyncNextFrame() {
    GetUDS()->net_frame_sync_impl->SendFrameReadyPacketAndWait();
}

bool NetFrameSynchronizer::IsEnabled() {
    return GetUDS()->net_frame_sync_impl->IsEnabled();
}

}; // namespace Network
