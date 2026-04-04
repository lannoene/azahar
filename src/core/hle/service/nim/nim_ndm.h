// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included..

#pragma once

#include <memory>
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::NIM {

class NIM_NDM final : public ServiceFramework<NIM_NDM> {
public:
    explicit NIM_NDM(Core::System& system);
    ~NIM_NDM();

private:
    SERVICE_SERIALIZATION_SIMPLE
    
    std::shared_ptr<Kernel::Event> daemon_event;
    
    void GetDaemonEventHandle(Kernel::HLERequestContext& ctx);
    void SetDaemonToWorkingState(Kernel::HLERequestContext& ctx);
    void GetDaemonState(Kernel::HLERequestContext& ctx);
};

} // namespace Service::NIM

SERVICE_CONSTRUCT(Service::NIM::NIM_NDM)
BOOST_CLASS_EXPORT_KEY(Service::NIM::NIM_NDM)
