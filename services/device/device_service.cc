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
#include "device/battery/battery_monitor.mojom.h"
#include "device/battery/battery_monitor_impl.h"
#include "device/battery/battery_status_service.h"
#include "device/sensors/device_sensor_host.h"
#include "device/wake_lock/wake_lock_context_provider.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/device/fingerprint/fingerprint.h"
#include "services/device/power_monitor/power_monitor_message_broadcaster.h"
#include "services/device/time_zone_monitor/time_zone_monitor.h"
#include "services/service_manager/public/cpp/connection.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/service_info.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_ANDROID)
#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "jni/InterfaceRegistrar_jni.h"
#include "services/device/android/register_jni.h"
#include "services/device/screen_orientation/screen_orientation_listener_android.h"
#else
#include "device/vibration/vibration_manager_impl.h"
#endif

namespace device {

#if defined(OS_ANDROID)
std::unique_ptr<service_manager::Service> CreateDeviceService(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    const WakeLockContextCallback& wake_lock_context_callback) {
  if (!EnsureJniRegistered()) {
    DLOG(ERROR) << "Failed to register JNI for Device Service";
    return nullptr;
  }

  return base::MakeUnique<DeviceService>(std::move(file_task_runner),
                                         std::move(io_task_runner),
                                         wake_lock_context_callback);
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
    const WakeLockContextCallback& wake_lock_context_callback)
    : java_interface_provider_initialized_(false),
      file_task_runner_(std::move(file_task_runner)),
      io_task_runner_(std::move(io_task_runner)),
      wake_lock_context_callback_(wake_lock_context_callback) {}
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

void DeviceService::OnStart() {}

bool DeviceService::OnConnect(const service_manager::ServiceInfo& remote_info,
                              service_manager::InterfaceRegistry* registry) {
  registry->AddInterface<mojom::Fingerprint>(this);
  registry->AddInterface<mojom::LightSensor>(this);
  registry->AddInterface<mojom::MotionSensor>(this);
  registry->AddInterface<mojom::OrientationSensor>(this);
  registry->AddInterface<mojom::OrientationAbsoluteSensor>(this);
  registry->AddInterface<mojom::PowerMonitor>(this);
  registry->AddInterface<mojom::ScreenOrientationListener>(this);
  registry->AddInterface<mojom::TimeZoneMonitor>(this);
  registry->AddInterface<mojom::WakeLockContextProvider>(this);

#if defined(OS_ANDROID)
  registry->AddInterface(
      GetJavaInterfaceProvider()->CreateInterfaceFactory<BatteryMonitor>());
  registry->AddInterface(
      GetJavaInterfaceProvider()
          ->CreateInterfaceFactory<mojom::VibrationManager>());
#else
  registry->AddInterface<BatteryMonitor>(this);
  registry->AddInterface<mojom::VibrationManager>(this);
#endif

  return true;
}

#if !defined(OS_ANDROID)
void DeviceService::Create(const service_manager::Identity& remote_identity,
                           BatteryMonitorRequest request) {
  device::BatteryMonitorImpl::Create(std::move(request));
}

void DeviceService::Create(const service_manager::Identity& remote_identity,
                           mojom::VibrationManagerRequest request) {
  VibrationManagerImpl::Create(std::move(request));
}
#endif

void DeviceService::Create(const service_manager::Identity& remote_identity,
                           mojom::FingerprintRequest request) {
  Fingerprint::Create(std::move(request));
}

void DeviceService::Create(const service_manager::Identity& remote_identity,
                           mojom::LightSensorRequest request) {
#if defined(OS_ANDROID)
  // On Android the device sensors implementations need to run on the UI thread
  // to communicate to Java.
  DeviceLightHost::Create(std::move(request));
#else
  // On platforms other than Android the device sensors implementations run on
  // the IO thread.
  if (io_task_runner_) {
    io_task_runner_->PostTask(FROM_HERE, base::Bind(&DeviceLightHost::Create,
                                                    base::Passed(&request)));
  }
#endif  // defined(OS_ANDROID)
}

void DeviceService::Create(const service_manager::Identity& remote_identity,
                           mojom::MotionSensorRequest request) {
#if defined(OS_ANDROID)
  // On Android the device sensors implementations need to run on the UI thread
  // to communicate to Java.
  DeviceMotionHost::Create(std::move(request));
#else
  // On platforms other than Android the device sensors implementations run on
  // the IO thread.
  if (io_task_runner_) {
    io_task_runner_->PostTask(FROM_HERE, base::Bind(&DeviceMotionHost::Create,
                                                    base::Passed(&request)));
  }
#endif  // defined(OS_ANDROID)
}

void DeviceService::Create(const service_manager::Identity& remote_identity,
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

void DeviceService::Create(const service_manager::Identity& remote_identity,
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

void DeviceService::Create(const service_manager::Identity& remote_identity,
                           mojom::PowerMonitorRequest request) {
  if (!power_monitor_message_broadcaster_) {
    power_monitor_message_broadcaster_ =
        base::MakeUnique<PowerMonitorMessageBroadcaster>();
  }
  power_monitor_message_broadcaster_->Bind(std::move(request));
}

void DeviceService::Create(const service_manager::Identity& remote_identity,
                           mojom::ScreenOrientationListenerRequest request) {
#if defined(OS_ANDROID)
  if (io_task_runner_) {
    io_task_runner_->PostTask(
        FROM_HERE, base::Bind(&ScreenOrientationListenerAndroid::Create,
                              base::Passed(&request)));
  }
#endif
}

void DeviceService::Create(const service_manager::Identity& remote_identity,
                           mojom::TimeZoneMonitorRequest request) {
  if (!time_zone_monitor_)
    time_zone_monitor_ = TimeZoneMonitor::Create(file_task_runner_);
  time_zone_monitor_->Bind(std::move(request));
}

void DeviceService::Create(const service_manager::Identity& remote_identity,
                           mojom::WakeLockContextProviderRequest request) {
  WakeLockContextProvider::Create(std::move(request), file_task_runner_,
                                  wake_lock_context_callback_);
}

#if defined(OS_ANDROID)
service_manager::InterfaceProvider* DeviceService::GetJavaInterfaceProvider() {
  if (!java_interface_provider_initialized_) {
    service_manager::mojom::InterfaceProviderPtr provider;
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_InterfaceRegistrar_createInterfaceRegistryForContext(
        env, mojo::MakeRequest(&provider).PassMessagePipe().release().value(),
        base::android::GetApplicationContext());
    java_interface_provider_.Bind(std::move(provider));

    java_interface_provider_initialized_ = true;
  }

  return &java_interface_provider_;
}
#endif

}  // namespace device
