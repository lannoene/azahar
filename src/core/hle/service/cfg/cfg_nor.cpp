// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/cfg/cfg_nor.h"
#include "core/hle/ipc_helpers.h"

SERIALIZE_EXPORT_IMPL(Service::CFG::CFG_NOR)

namespace Service::CFG {

void CFG_NOR::Initialize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    auto v = rp.Pop<u8>();
    
    LOG_INFO(Service_CFG, "called v={}", v);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
    
}

void CFG_NOR::Shutdown(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    
    LOG_INFO(Service_CFG, "called");

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void CFG_NOR::ReadData(Kernel::HLERequestContext& ctx) {
    LOG_INFO(Service_CFG, "called");
    
    IPC::RequestParser rp(ctx);
    auto offset = rp.Pop<u32>();
    auto size = rp.Pop<u32>();
    auto out_buffer = rp.PopMappedBuffer();
    
    
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushMappedBuffer(out_buffer);
}

CFG_NOR::CFG_NOR() : ServiceFramework("cfg:nor", 23) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, &CFG_NOR::Initialize, "Initialize"},
        {0x0002, &CFG_NOR::Shutdown, "Shutdown"},
        {0x0005, &CFG_NOR::ReadData, "ReadData"},
        {0x0006, nullptr, "WriteData"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::CFG
