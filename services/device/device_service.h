// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_DEVICE_SERVICE_H_
#define SERVICES_DEVICE_DEVICE_SERVICE_H_

#include "base/memory/ref_counted.h"
#include "device/battery/battery_monitor.mojom.h"
#include "device/screen_orientation/public/interfaces/screen_orientation.mojom.h"
#include "device/sensors/public/interfaces/light.mojom.h"
#include "device/sensors/public/interfaces/motion.mojom.h"
#include "device/sensors/public/interfaces/orientation.mojom.h"
#include "device/vibration/vibration_manager.mojom.h"
#include "device/wake_lock/public/interfaces/wake_lock_context_provider.mojom.h"
#include "device/wake_lock/wake_lock_service_context.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/device/public/interfaces/fingerprint.mojom.h"
#include "services/device/public/interfaces/power_monitor.mojom.h"
#include "services/device/public/interfaces/time_zone_monitor.mojom.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/public/cpp/service.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace device {

class PowerMonitorMessageBroadcaster;
class TimeZoneMonitor;

#if defined(OS_ANDROID)
std::unique_ptr<service_manager::Service> CreateDeviceService(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    const WakeLockContextCallback& wake_lock_context_callback);
#else
std::unique_ptr<service_manager::Service> CreateDeviceService(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
#endif

class DeviceService
    : public service_manager::Service,
      public service_manager::InterfaceFactory<mojom::Fingerprint>,
      public service_manager::InterfaceFactory<mojom::LightSensor>,
      public service_manager::InterfaceFactory<mojom::MotionSensor>,
      public service_manager::InterfaceFactory<mojom::OrientationSensor>,
      public service_manager::InterfaceFactory<
          mojom::OrientationAbsoluteSensor>,
#if !defined(OS_ANDROID)
      // On Android the Device Service provides BatteryMonitor via Java.
      public service_manager::InterfaceFactory<BatteryMonitor>,
      // On Android the Device Service provides VibrationManager via Java.
      public service_manager::InterfaceFactory<mojom::VibrationManager>,
#endif
      public service_manager::InterfaceFactory<mojom::PowerMonitor>,
      public service_manager::InterfaceFactory<
          mojom::ScreenOrientationListener>,
      public service_manager::InterfaceFactory<mojom::TimeZoneMonitor>,
      public service_manager::InterfaceFactory<mojom::WakeLockContextProvider> {
 public:
#if defined(OS_ANDROID)
  DeviceService(scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
                scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
                const WakeLockContextCallback& wake_lock_context_callback);
#else
  DeviceService(scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
                scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
#endif
  ~DeviceService() override;

 private:
  // service_manager::Service:
  void OnStart() override;
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override;

  // InterfaceFactory<mojom::Fingerprint>:
  void Create(const service_manager::Identity& remote_identity,
              mojom::FingerprintRequest request) override;

  // InterfaceFactory<mojom::LightSensor>:
  void Create(const service_manager::Identity& remote_identity,
              mojom::LightSensorRequest request) override;

  // InterfaceFactory<mojom::MotionSensor>:
  void Create(const service_manager::Identity& remote_identity,
              mojom::MotionSensorRequest request) override;

  // InterfaceFactory<mojom::OrientationSensor>:
  void Create(const service_manager::Identity& remote_identity,
              mojom::OrientationSensorRequest request) override;

  // InterfaceFactory<mojom::OrientationAbsolueSensor>:
  void Create(const service_manager::Identity& remote_identity,
              mojom::OrientationAbsoluteSensorRequest request) override;

#if !defined(OS_ANDROID)
  // InterfaceFactory<BatteryMonitor>:
  void Create(const service_manager::Identity& remote_identity,
              BatteryMonitorRequest request) override;
  // InterfaceFactory<mojom::VibrationManager>:
  void Create(const service_manager::Identity& remote_identity,
              mojom::VibrationManagerRequest request) override;
#endif

  // InterfaceFactory<mojom::PowerMonitor>:
  void Create(const service_manager::Identity& remote_identity,
              mojom::PowerMonitorRequest request) override;

  // InterfaceFactory<mojom::ScreenOrientationListener>:
  void Create(const service_manager::Identity& remote_identity,
              mojom::ScreenOrientationListenerRequest request) override;

  // InterfaceFactory<mojom::TimeZoneMonitor>:
  void Create(const service_manager::Identity& remote_identity,
              mojom::TimeZoneMonitorRequest request) override;

  // InterfaceFactory<mojom::WakeLockContextProvider>:
  void Create(const service_manager::Identity& remote_identity,
              mojom::WakeLockContextProviderRequest request) override;

  std::unique_ptr<PowerMonitorMessageBroadcaster>
      power_monitor_message_broadcaster_;
  std::unique_ptr<TimeZoneMonitor> time_zone_monitor_;
#if defined(OS_ANDROID)
  // Binds |java_interface_provider_| to an interface registry that exposes
  // factories for the interfaces that are provided via Java on Android.
  service_manager::InterfaceProvider* GetJavaInterfaceProvider();

  // InterfaceProvider that is bound to the Java-side interface registry.
  service_manager::InterfaceProvider java_interface_provider_;

  bool java_interface_provider_initialized_;
#endif

  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  WakeLockContextCallback wake_lock_context_callback_;

  DISALLOW_COPY_AND_ASSIGN(DeviceService);
};

}  // namespace device

#endif  // SERVICES_DEVICE_DEVICE_SERVICE_H_
