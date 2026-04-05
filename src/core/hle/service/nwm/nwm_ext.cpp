// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/nwm/nwm_ext.h"
#include "core/hle/service/sm/srv.h"
#include "core/hle/ipc_helpers.h"
#include "core/core.h"
#include "core/hle/kernel/shared_page.h"

SERIALIZE_EXPORT_IMPL(Service::NWM::NWM_EXT)

namespace Service::NWM {

void NWM_EXT::ControlWifiEnabled(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    bool disable = rp.Pop<bool>();
    
    LOG_DEBUG(Service_NWM, "called disable={}", disable);
    
    Core::System::GetInstance().Kernel().GetSharedPageHandler().SetWifiState(!disable ? SharedPage::WifiState::Enabled : SharedPage::WifiState::Disabled);
    Core::System::GetInstance().Kernel().GetSharedPageHandler().SetWifiLinkLevel(SharedPage::WifiLinkLevel::Off);
    
    Core::System::GetInstance().ServiceManager().GetService<SM::SRV>("srv:")->PublishToSubscribers(0x302);
    if (disable)
        Core::System::GetInstance().ServiceManager().GetService<SM::SRV>("srv:")->PublishToSubscribers(0x303);
    else
        Core::System::GetInstance().ServiceManager().GetService<SM::SRV>("srv:")->PublishToSubscribers(0x304);
    
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void NWM_EXT::Unk1(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_NWM, "called");
    IPC::RequestParser rp(ctx);
    
    auto unk1 = rp.PopStaticBuffer();
    
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

NWM_EXT::NWM_EXT() : ServiceFramework("nwm::EXT") {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0005, &NWM_EXT::Unk1, "Unk1"},
        {0x0008, &NWM_EXT::ControlWifiEnabled, "ControlWirelessEnabled"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::NWM
