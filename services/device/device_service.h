// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_DEVICE_SERVICE_H_
#define SERVICES_DEVICE_DEVICE_SERVICE_H_

#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/device/public/interfaces/fingerprint.mojom.h"
#include "services/device/public/interfaces/power_monitor.mojom.h"
#include "services/device/public/interfaces/time_zone_monitor.mojom.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/service.h"

namespace device {

class TimeZoneMonitor;

std::unique_ptr<service_manager::Service> CreateDeviceService(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner);

class DeviceService
    : public service_manager::Service,
      public service_manager::InterfaceFactory<mojom::Fingerprint>,
      public service_manager::InterfaceFactory<mojom::PowerMonitor>,
      public service_manager::InterfaceFactory<mojom::TimeZoneMonitor> {
 public:
  DeviceService(scoped_refptr<base::SingleThreadTaskRunner> file_task_runner);
  ~DeviceService() override;

 private:
  // service_manager::Service:
  void OnStart() override;
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override;

  // InterfaceFactory<mojom::Fingerprint>:
  void Create(const service_manager::Identity& remote_identity,
              mojom::FingerprintRequest request) override;

  // InterfaceFactory<mojom::PowerMonitor>:
  void Create(const service_manager::Identity& remote_identity,
              mojom::PowerMonitorRequest request) override;

  // InterfaceFactory<mojom::TimeZoneMonitor>:
  void Create(const service_manager::Identity& remote_identity,
              mojom::TimeZoneMonitorRequest request) override;

  std::unique_ptr<device::TimeZoneMonitor> time_zone_monitor_;

  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(DeviceService);
};

}  // namespace device

#endif  // SERVICES_DEVICE_DEVICE_SERVICE_H_
