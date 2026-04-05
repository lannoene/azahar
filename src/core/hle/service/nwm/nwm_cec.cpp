// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/nwm/nwm_cec.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/sm/srv.h"
#include "core/core.h"

SERIALIZE_EXPORT_IMPL(Service::NWM::NWM_CEC)

namespace Service::NWM {

void NWM_CEC::UnkStart(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_NWM, "called");
    IPC::RequestParser rp(ctx);
    
    auto unk1 = rp.Pop<u32>();
    auto mapped_buf = rp.PopMappedBuffer();
    
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

NWM_CEC::NWM_CEC() : ServiceFramework("nwm::CEC") {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x000D, nullptr, "SendProbeRequest"},
        {0x0014, &NWM_CEC::UnkStart, "UnkStart"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::NWM
