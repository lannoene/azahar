// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"
#include "network/network.h"

namespace Service::NWM {

class NWM_INF final : public ServiceFramework<NWM_INF> {
public:
    NWM_INF();

private:
    SERVICE_SERIALIZATION_SIMPLE
    
    std::list<Network::WifiPacket> GetReceivedBeacons(const Network::MacAddress& sender);
    
    void Initialize(Kernel::HLERequestContext& ctx);
    void Finalize(Kernel::HLERequestContext& ctx);
    void UnkScanForBeacons(Kernel::HLERequestContext& ctx);
    void RecvBeaconBroadcastData(Kernel::HLERequestContext& ctx);
    void Unk1(Kernel::HLERequestContext& ctx);
    
    std::list<Network::WifiPacket> received_beacons;
};

} // namespace Service::NWM

BOOST_CLASS_EXPORT_KEY(Service::NWM::NWM_INF)
