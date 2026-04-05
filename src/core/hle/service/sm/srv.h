// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <queue>
#include <memory>
#include <unordered_map>
#include <boost/serialization/export.hpp>
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Kernel {
class HLERequestContext;
class Semaphore;
} // namespace Kernel

namespace Service::SM {

/// Interface to "srv:" service
class SRV final : public ServiceFramework<SRV> {
public:
    explicit SRV(Core::System& system);
    ~SRV();

    void PublishToSubscribers(u16 notif);

    class ThreadCallback;

private:
    void RegisterClient(Kernel::HLERequestContext& ctx);
    void EnableNotification(Kernel::HLERequestContext& ctx);
    void GetServiceHandle(Kernel::HLERequestContext& ctx);
    void Subscribe(Kernel::HLERequestContext& ctx);
    void Unsubscribe(Kernel::HLERequestContext& ctx);
    void PublishToSubscriber(Kernel::HLERequestContext& ctx);
    void RegisterService(Kernel::HLERequestContext& ctx);
    void ReceiveNotification(Kernel::HLERequestContext& ctx);
    
    struct NotificationReceiver {
        std::shared_ptr<Kernel::Semaphore> sm_notify;
        std::queue<u16> notifications;
    };

    Core::System& system;
    std::shared_ptr<Kernel::Semaphore> notification_semaphore;
    std::unordered_map<std::string, std::vector<std::shared_ptr<Kernel::Event>>> get_service_handle_delayed_map;
    std::unordered_map<u16, std::vector<std::shared_ptr<Kernel::ClientSession>>> notification_subscribers_map;
    std::unordered_map<std::shared_ptr<Kernel::ClientSession>, NotificationReceiver> notification_semaphore_map;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
    friend class boost::serialization::access;
};

} // namespace Service::SM

SERVICE_CONSTRUCT(Service::SM::SRV)
BOOST_CLASS_EXPORT_KEY(Service::SM::SRV)
BOOST_CLASS_EXPORT_KEY(Service::SM::SRV::ThreadCallback)
