// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/nwm/nwm_inf.h"
#include "core/hle/service/nwm/uds_common.h"
#include "core/hle/service/nwm/uds_beacon.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/event.h"

SERIALIZE_EXPORT_IMPL(Service::NWM::NWM_INF)

namespace Service::NWM {

std::list<Network::WifiPacket> NWM_INF::GetReceivedBeacons(const MacAddress& sender) {
    //std::scoped_lock lock(beacon_mutex);
    if (sender != Network::BroadcastMac) {
        std::list<Network::WifiPacket> filtered_list;
        const auto beacon = std::find_if(received_beacons.begin(), received_beacons.end(),
                                         [&sender](const Network::WifiPacket& packet) {
                                             return packet.transmitter_address == sender;
                                         });
        if (beacon != received_beacons.end()) {
            filtered_list.push_back(*beacon);
            // TODO(B3N30): Check if the complete deque is cleared or just the fetched entries
            received_beacons.erase(beacon);
        }
        return filtered_list;
    }
    received_beacons = {

    };
    return std::move(received_beacons);
}

void NWM_INF::Initialize(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_NWM, "called");
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void NWM_INF::Finalize(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_NWM, "called");
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

// sleeps for x sec then returns and prog should call recvbeac...data
void NWM_INF::UnkScanForBeacons(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_NWM, "called");
    IPC::RequestParser rp(ctx);
    
    u16 unk1 = 3; // can be 1 2 or 3, possibly which wifi we are connected to?
    u16 unk2 = 1;

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);
    rb.Push(ResultSuccess);
    rb.Push(unk1);
    rb.Push(unk2);
}

struct APScanInfo {
    std::array<u8, 0x34> unk;
};
static_assert(sizeof(APScanInfo) == 0x34);

// THIS IS THE SUPER RecvBeaconBroadcastData. TODO: PUT THIS IN NWM MODULE INTERFACE TO USE IN CEC TOO!!!
// make uds RecvBeaconBroadcastData a wrapper to this like in the real nwm.
void NWM_INF::RecvBeaconBroadcastData(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_NWM, "called");
    IPC::RequestParser rp(ctx);

    u32 out_buffer_size = rp.Pop<u32>();
    u32 unk1 = rp.Pop<u32>();

    // scan input struct
    auto ap_scan_info = rp.PopRaw<APScanInfo>();
    // From 3dbrew:
    // 'Official user processes create a new event handle which is then passed to this command.
    // However, those user processes don't save that handle anywhere afterwards.'
    // So we don't save/use that event too.
    std::shared_ptr<Kernel::Event> input_event = rp.PopObject<Kernel::Event>();

    Kernel::MappedBuffer& out_buffer = rp.PopMappedBuffer();
    ASSERT(out_buffer.GetSize() == out_buffer_size);

    std::size_t cur_buffer_size = sizeof(BeaconDataReplyHeader);

    auto beacons = GetReceivedBeacons(Network::BroadcastMac);

    BeaconDataReplyHeader data_reply_header{};
    data_reply_header.total_entries = static_cast<u32>(beacons.size());
    data_reply_header.max_output_size = out_buffer_size;

    // Write each of the received beacons into the buffer
    for (const auto& beacon : beacons) {
        BeaconEntryHeader entry{};
        // TODO(Subv): Figure out what this size is used for.
        entry.unk_size = static_cast<u32>(sizeof(BeaconEntryHeader) + beacon.data.size());
        entry.total_size = static_cast<u32>(sizeof(BeaconEntryHeader) + beacon.data.size());
        entry.wifi_channel = beacon.channel;
        entry.header_size = sizeof(BeaconEntryHeader);
        entry.mac_address = beacon.transmitter_address;

        ASSERT(cur_buffer_size < out_buffer_size);

        out_buffer.Write(&entry, cur_buffer_size, sizeof(BeaconEntryHeader));
        cur_buffer_size += sizeof(BeaconEntryHeader);
        const unsigned char* beacon_data = beacon.data.data();
        out_buffer.Write(beacon_data, cur_buffer_size, beacon.data.size());
        cur_buffer_size += beacon.data.size();
    }

    // Update the total size in the structure and write it to the buffer again.
    data_reply_header.total_size = static_cast<u32>(cur_buffer_size);
    out_buffer.Write(&data_reply_header, 0, sizeof(BeaconDataReplyHeader));

    // on a real 3ds this is about 0.38 seconds
    static constexpr std::chrono::nanoseconds UDSBeaconScanInterval{3000000000};

    ctx.SleepClientThread("inf::RecvBeaconBroadcastData", UDSBeaconScanInterval, nullptr);

//    LOG_DEBUG(Service_NWM,
//              "called out_buffer_size=0x{:08X}, wlan_comm_id=0x{:08X}, id=0x{:08X},"
//              "unk1=0x{:08X}, unk2=0x{:08X}, offset={}",
//              out_buffer_size, wlan_comm_id, id, unk1, unk2, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushMappedBuffer(out_buffer);
}

void NWM_INF::Unk1(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_NWM, "called");
    IPC::RequestParser rp(ctx);
    
    auto unk1 = rp.PopStaticBuffer();
    
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

NWM_INF::NWM_INF() : ServiceFramework("nwm::INF") {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0003, &NWM_INF::Initialize, "UnkStart"},
        {0x0004, &NWM_INF::Finalize, "UnkFinish"},
        {0x0005, &NWM_INF::UnkScanForBeacons, "UnkScanForBeacons"},
        {0x0006, &NWM_INF::RecvBeaconBroadcastData, "RecvBeaconBroadcastData"},
        {0x0007, nullptr, "ConnectToEncryptedAP"},
        {0x0008, nullptr, "ConnectToAP"},
        {0x000e, &NWM_INF::Unk1, "Unk1"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::NWM
