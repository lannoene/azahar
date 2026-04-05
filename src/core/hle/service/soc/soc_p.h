// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included..

#pragma once

#include <memory>
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::SOC {

class SOC_P final : public ServiceFramework<SOC_P> {
public:
    explicit SOC_P(Core::System& system);
    ~SOC_P();

private:
    SERVICE_SERIALIZATION_SIMPLE
    
    std::shared_ptr<Kernel::Event> event;
    
    void GetSocEventHandle(Kernel::HLERequestContext& ctx);
};

} // namespace Service::SOC

SERVICE_CONSTRUCT(Service::SOC::SOC_P)
BOOST_CLASS_EXPORT_KEY(Service::SOC::SOC_P)
