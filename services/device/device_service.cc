// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/device_service.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/generic_sensor/sensor_provider_impl.h"
#include "device/sensors/device_sensor_host.h"
#include "device/wake_lock/wake_lock_provider.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/device/fingerprint/fingerprint.h"
#include "services/device/power_monitor/power_monitor_message_broadcaster.h"
#include "services/device/public/interfaces/battery_monitor.mojom.h"
#include "services/device/time_zone_monitor/time_zone_monitor.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "jni/InterfaceRegistrar_jni.h"
#include "services/device/android/register_jni.h"
#include "services/device/screen_orientation/screen_orientation_listener_android.h"
#else
#include "services/device/battery/battery_monitor_impl.h"
#include "services/device/battery/battery_status_service.h"
#include "services/device/vibration/vibration_manager_impl.h"
#endif

namespace device {

#if defined(OS_ANDROID)
std::unique_ptr<service_manager::Service> CreateDeviceService(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    const WakeLockContextCallback& wake_lock_context_callback,
    const base::android::JavaRef<jobject>& java_nfc_delegate) {
  if (!EnsureJniRegistered()) {
    DLOG(ERROR) << "Failed to register JNI for Device Service";
    return nullptr;
  }

  return base::MakeUnique<DeviceService>(
      std::move(file_task_runner), std::move(io_task_runner),
      wake_lock_context_callback, java_nfc_delegate);
}
#else
std::unique_ptr<service_manager::Service> CreateDeviceService(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
  return base::MakeUnique<DeviceService>(std::move(file_task_runner),
                                         std::move(io_task_runner));
}
#endif

#if defined(OS_ANDROID)
DeviceService::DeviceService(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    const WakeLockContextCallback& wake_lock_context_callback,
    const base::android::JavaRef<jobject>& java_nfc_delegate)
    : file_task_runner_(std::move(file_task_runner)),
      io_task_runner_(std::move(io_task_runner)),
      wake_lock_context_callback_(wake_lock_context_callback),
      java_interface_provider_initialized_(false) {
  java_nfc_delegate_.Reset(java_nfc_delegate);
}
#else
DeviceService::DeviceService(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : file_task_runner_(std::move(file_task_runner)),
      io_task_runner_(std::move(io_task_runner)) {}
#endif

DeviceService::~DeviceService() {
#if !defined(OS_ANDROID)
  device::BatteryStatusService::GetInstance()->Shutdown();
#endif
}

void DeviceService::OnStart() {
  registry_.AddInterface<mojom::Fingerprint>(base::Bind(
      &DeviceService::BindFingerprintRequest, base::Unretained(this)));
  registry_.AddInterface<mojom::OrientationSensor>(base::Bind(
      &DeviceService::BindOrientationSensorRequest, base::Unretained(this)));
  registry_.AddInterface<mojom::OrientationAbsoluteSensor>(
      base::Bind(&DeviceService::BindOrientationAbsoluteSensorRequest,
                 base::Unretained(this)));
  registry_.AddInterface<mojom::PowerMonitor>(base::Bind(
      &DeviceService::BindPowerMonitorRequest, base::Unretained(this)));
  registry_.AddInterface<mojom::ScreenOrientationListener>(
      base::Bind(&DeviceService::BindScreenOrientationListenerRequest,
                 base::Unretained(this)));
  registry_.AddInterface<mojom::SensorProvider>(base::Bind(
      &DeviceService::BindSensorProviderRequest, base::Unretained(this)));
  registry_.AddInterface<mojom::TimeZoneMonitor>(base::Bind(
      &DeviceService::BindTimeZoneMonitorRequest, base::Unretained(this)));
  registry_.AddInterface<mojom::WakeLockProvider>(base::Bind(
      &DeviceService::BindWakeLockProviderRequest, base::Unretained(this)));

#if defined(OS_ANDROID)
  registry_.AddInterface(GetJavaInterfaceProvider()
                             ->CreateInterfaceFactory<mojom::BatteryMonitor>());
  registry_.AddInterface(
      GetJavaInterfaceProvider()->CreateInterfaceFactory<mojom::NFCProvider>());
  registry_.AddInterface(
      GetJavaInterfaceProvider()
          ->CreateInterfaceFactory<mojom::VibrationManager>());
#else
  registry_.AddInterface<mojom::BatteryMonitor>(base::Bind(
      &DeviceService::BindBatteryMonitorRequest, base::Unretained(this)));
  registry_.AddInterface<mojom::NFCProvider>(base::Bind(
      &DeviceService::BindNFCProviderRequest, base::Unretained(this)));
  registry_.AddInterface<mojom::VibrationManager>(base::Bind(
      &DeviceService::BindVibrationManagerRequest, base::Unretained(this)));
#endif
}

void DeviceService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(source_info, interface_name,
                          std::move(interface_pipe));
}

#if !defined(OS_ANDROID)
void DeviceService::BindBatteryMonitorRequest(
    const service_manager::BindSourceInfo& source_info,
    mojom::BatteryMonitorRequest request) {
  BatteryMonitorImpl::Create(std::move(request));
}

void DeviceService::BindNFCProviderRequest(
    const service_manager::BindSourceInfo& source_info,
    mojom::NFCProviderRequest request) {
  LOG(ERROR) << "NFC is only supported on Android";
  NOTREACHED();
}

void DeviceService::BindVibrationManagerRequest(
    const service_manager::BindSourceInfo& source_info,
    mojom::VibrationManagerRequest request) {
  VibrationManagerImpl::Create(std::move(request));
}
#endif

void DeviceService::BindFingerprintRequest(
    const service_manager::BindSourceInfo& source_info,
    mojom::FingerprintRequest request) {
  Fingerprint::Create(std::move(request));
}

void DeviceService::BindOrientationSensorRequest(
    const service_manager::BindSourceInfo& source_info,
    mojom::OrientationSensorRequest request) {
#if defined(OS_ANDROID)
  // On Android the device sensors implementations need to run on the UI thread
  // to communicate to Java.
  DeviceOrientationHost::Create(std::move(request));
#else
  // On platforms other than Android the device sensors implementations run on
  // the IO thread.
  if (io_task_runner_) {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DeviceOrientationHost::Create, base::Passed(&request)));
  }
#endif  // defined(OS_ANDROID)
}

void DeviceService::BindOrientationAbsoluteSensorRequest(
    const service_manager::BindSourceInfo& source_info,
    mojom::OrientationAbsoluteSensorRequest request) {
#if defined(OS_ANDROID)
  // On Android the device sensors implementations need to run on the UI thread
  // to communicate to Java.
  DeviceOrientationAbsoluteHost::Create(std::move(request));
#else
  // On platforms other than Android the device sensors implementations run on
  // the IO thread.
  if (io_task_runner_) {
    io_task_runner_->PostTask(FROM_HERE,
                              base::Bind(&DeviceOrientationAbsoluteHost::Create,
                                         base::Passed(&request)));
  }
#endif  // defined(OS_ANDROID)
}

void DeviceService::BindPowerMonitorRequest(
    const service_manager::BindSourceInfo& source_info,
    mojom::PowerMonitorRequest request) {
  if (!power_monitor_message_broadcaster_) {
    power_monitor_message_broadcaster_ =
        base::MakeUnique<PowerMonitorMessageBroadcaster>();
  }
  power_monitor_message_broadcaster_->Bind(std::move(request));
}

void DeviceService::BindScreenOrientationListenerRequest(
    const service_manager::BindSourceInfo& source_info,
    mojom::ScreenOrientationListenerRequest request) {
#if defined(OS_ANDROID)
  if (io_task_runner_) {
    io_task_runner_->PostTask(
        FROM_HERE, base::Bind(&ScreenOrientationListenerAndroid::Create,
                              base::Passed(&request)));
  }
#endif
}

void DeviceService::BindSensorProviderRequest(
    const service_manager::BindSourceInfo& source_info,
    mojom::SensorProviderRequest request) {
  if (io_task_runner_) {
    io_task_runner_->PostTask(
        FROM_HERE, base::Bind(&device::SensorProviderImpl::Create,
                              file_task_runner_, base::Passed(&request)));
  }
}

void DeviceService::BindTimeZoneMonitorRequest(
    const service_manager::BindSourceInfo& source_info,
    mojom::TimeZoneMonitorRequest request) {
  if (!time_zone_monitor_)
    time_zone_monitor_ = TimeZoneMonitor::Create(file_task_runner_);
  time_zone_monitor_->Bind(std::move(request));
}

void DeviceService::BindWakeLockProviderRequest(
    const service_manager::BindSourceInfo& source_info,
    mojom::WakeLockProviderRequest request) {
  WakeLockProvider::Create(std::move(request), file_task_runner_,
                           wake_lock_context_callback_);
}

#if defined(OS_ANDROID)
service_manager::InterfaceProvider* DeviceService::GetJavaInterfaceProvider() {
  if (!java_interface_provider_initialized_) {
    service_manager::mojom::InterfaceProviderPtr provider;
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_InterfaceRegistrar_createInterfaceRegistryForContext(
        env, mojo::MakeRequest(&provider).PassMessagePipe().release().value(),
        java_nfc_delegate_);
    java_interface_provider_.Bind(std::move(provider));

    java_interface_provider_initialized_ = true;
  }

  return &java_interface_provider_;
}
#endif

}  // namespace device
