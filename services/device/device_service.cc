// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/device_service.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/device/fingerprint/fingerprint.h"
#include "services/device/power_monitor/power_monitor_message_broadcaster.h"
#include "services/device/time_zone_monitor/time_zone_monitor.h"
#include "services/service_manager/public/cpp/connection.h"
#include "services/service_manager/public/cpp/interface_registry.h"

#if defined(OS_ANDROID)
#include "services/device/android/register_jni.h"
#endif

namespace device {

std::unique_ptr<service_manager::Service> CreateDeviceService(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner) {
#if defined(OS_ANDROID)
  if (!EnsureJniRegistered()) {
    DLOG(ERROR) << "Failed to register JNI for Device Service";
    return nullptr;
  }
#endif

  return base::MakeUnique<DeviceService>(std::move(file_task_runner));
}

DeviceService::DeviceService(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner)
    : file_task_runner_(std::move(file_task_runner)) {}

DeviceService::~DeviceService() {}

void DeviceService::OnStart() {}

bool DeviceService::OnConnect(const service_manager::ServiceInfo& remote_info,
                              service_manager::InterfaceRegistry* registry) {
  registry->AddInterface<mojom::Fingerprint>(this);
  registry->AddInterface<mojom::PowerMonitor>(this);
  registry->AddInterface<mojom::TimeZoneMonitor>(this);
  return true;
}

void DeviceService::Create(const service_manager::Identity& remote_identity,
                           mojom::FingerprintRequest request) {
  Fingerprint::Create(std::move(request));
}

void DeviceService::Create(const service_manager::Identity& remote_identity,
                           mojom::PowerMonitorRequest request) {
  PowerMonitorMessageBroadcaster::Create(std::move(request));
}

void DeviceService::Create(const service_manager::Identity& remote_identity,
                           mojom::TimeZoneMonitorRequest request) {
  if (!time_zone_monitor_)
    time_zone_monitor_ = device::TimeZoneMonitor::Create(file_task_runner_);
  time_zone_monitor_->Bind(std::move(request));
}

}  // namespace device
