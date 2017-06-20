// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_DEVICE_SERVICE_H_
#define SERVICES_DEVICE_DEVICE_SERVICE_H_

#include "base/memory/ref_counted.h"
#include "device/generic_sensor/public/interfaces/sensor_provider.mojom.h"
#include "device/screen_orientation/public/interfaces/screen_orientation.mojom.h"
#include "device/sensors/public/interfaces/orientation.mojom.h"
#include "device/wake_lock/public/interfaces/wake_lock_provider.mojom.h"
#include "device/wake_lock/wake_lock_context.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/device/public/interfaces/battery_monitor.mojom.h"
#include "services/device/public/interfaces/fingerprint.mojom.h"
#include "services/device/public/interfaces/nfc_provider.mojom.h"
#include "services/device/public/interfaces/power_monitor.mojom.h"
#include "services/device/public/interfaces/time_zone_monitor.mojom.h"
#include "services/device/public/interfaces/vibration_manager.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/public/cpp/service.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

namespace base {
class SingleThreadTaskRunner;
}

namespace device {

class PowerMonitorMessageBroadcaster;
class TimeZoneMonitor;

#if defined(OS_ANDROID)
// NOTE: See the comments on the definitions of |WakeLockContextCallback|
// and NFCDelegate.java to understand the semantics and usage of these
// parameters.
std::unique_ptr<service_manager::Service> CreateDeviceService(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    const WakeLockContextCallback& wake_lock_context_callback,
    const base::android::JavaRef<jobject>& java_nfc_delegate);
#else
std::unique_ptr<service_manager::Service> CreateDeviceService(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
#endif

class DeviceService : public service_manager::Service {
 public:
#if defined(OS_ANDROID)
  DeviceService(scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
                scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
                const WakeLockContextCallback& wake_lock_context_callback,
                const base::android::JavaRef<jobject>& java_nfc_delegate);
#else
  DeviceService(scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
                scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
#endif
  ~DeviceService() override;

 private:
  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  void BindFingerprintRequest(
      const service_manager::BindSourceInfo& source_info,
      mojom::FingerprintRequest request);

  void BindOrientationSensorRequest(
      const service_manager::BindSourceInfo& source_info,
      mojom::OrientationSensorRequest request);

  void BindOrientationAbsoluteSensorRequest(
      const service_manager::BindSourceInfo& source_info,
      mojom::OrientationAbsoluteSensorRequest request);

#if !defined(OS_ANDROID)
  void BindBatteryMonitorRequest(
      const service_manager::BindSourceInfo& source_info,
      mojom::BatteryMonitorRequest request);
  void BindNFCProviderRequest(
      const service_manager::BindSourceInfo& source_info,
      mojom::NFCProviderRequest request);
  void BindVibrationManagerRequest(
      const service_manager::BindSourceInfo& source_info,
      mojom::VibrationManagerRequest request);
#endif

  void BindPowerMonitorRequest(
      const service_manager::BindSourceInfo& source_info,
      mojom::PowerMonitorRequest request);

  void BindScreenOrientationListenerRequest(
      const service_manager::BindSourceInfo& source_info,
      mojom::ScreenOrientationListenerRequest request);

  void BindSensorProviderRequest(
      const service_manager::BindSourceInfo& source_info,
      mojom::SensorProviderRequest request);

  void BindTimeZoneMonitorRequest(
      const service_manager::BindSourceInfo& source_info,
      mojom::TimeZoneMonitorRequest request);

  void BindWakeLockProviderRequest(
      const service_manager::BindSourceInfo& source_info,
      mojom::WakeLockProviderRequest request);

  std::unique_ptr<PowerMonitorMessageBroadcaster>
      power_monitor_message_broadcaster_;
  std::unique_ptr<TimeZoneMonitor> time_zone_monitor_;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  WakeLockContextCallback wake_lock_context_callback_;

#if defined(OS_ANDROID)
  // Binds |java_interface_provider_| to an interface registry that exposes
  // factories for the interfaces that are provided via Java on Android.
  service_manager::InterfaceProvider* GetJavaInterfaceProvider();

  // InterfaceProvider that is bound to the Java-side interface registry.
  service_manager::InterfaceProvider java_interface_provider_;

  bool java_interface_provider_initialized_;

  base::android::ScopedJavaGlobalRef<jobject> java_nfc_delegate_;
#endif

  service_manager::BinderRegistry registry_;

  DISALLOW_COPY_AND_ASSIGN(DeviceService);
};

}  // namespace device

#endif  // SERVICES_DEVICE_DEVICE_SERVICE_H_
