// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/nim/nim_ndm.h"
#include "core/hle/kernel/event.h"
#include "core/hle/ipc_helpers.h"
#include "core/core.h"

SERVICE_CONSTRUCT_IMPL(Service::NIM::NIM_NDM)
SERIALIZE_EXPORT_IMPL(Service::NIM::NIM_NDM)

namespace Service::NIM {

void NIM_NDM::GetDaemonEventHandle(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(daemon_event);

    LOG_WARNING(Service_NIM, "(STUBBED) called");
}

NIM_NDM::NIM_NDM(Core::System& system) : ServiceFramework("nim:ndm", 2) {
    const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, &NIM_NDM::GetDaemonEventHandle, "GetDaemonEventHandle"},
        {0x0002, nullptr, "Stub"},
        {0x0003, nullptr, "SetDaemonToWorkingState"},
        {0x0004, nullptr, "SetDaemonToSuspendedState"},
        {0x0005, nullptr, "GetDaemonState"},
        // clang-format on
    };
    RegisterHandlers(functions);
    
    daemon_event = system.Kernel().CreateEvent(Kernel::ResetType::OneShot, "nim_ndm_daemon_even");
}

NIM_NDM::~NIM_NDM() = default;

} // namespace Service::NIM
