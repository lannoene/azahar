// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/soc/soc_p.h"
#include "core/hle/kernel/event.h"
#include "core/hle/ipc_helpers.h"
#include "core/core.h"

SERVICE_CONSTRUCT_IMPL(Service::SOC::SOC_P)
SERIALIZE_EXPORT_IMPL(Service::SOC::SOC_P)

namespace Service::SOC {

void SOC_P::GetDaemonEventHandle(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_WARNING(Service_NIM, "(STUBBED) called");
}

SOC_P::SOC_P(Core::System& system) : ServiceFramework("soc:P", 3) {
    const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, nullptr, "InitializeSockets"},
        {0x0002, nullptr, "FinalizeSockets"},
        {0x0003, nullptr, "SetNetworkOpt"},
        {0x0004, nullptr, "CloseSocketsForProcess"},
        {0x0005, nullptr, "unk1"},
        {0x0006, nullptr, "gethostid"},
        {0x0007, nullptr, "unk2"},
        {0x0008, nullptr, "unk3"},
        {0x0009, nullptr, "StopInitializeSockets"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

SOC_P::~SOC_P() = default;

} // namespace Service::SOC
