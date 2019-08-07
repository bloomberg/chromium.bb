// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_DEVICE_SERVICE_H_
#define SERVICES_DEVICE_DEVICE_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "device/usb/mojo/device_manager_impl.h"
#include "device/usb/mojo/device_manager_test.h"
#include "device/usb/public/mojom/device_manager.mojom.h"
#include "device/usb/public/mojom/device_manager_test.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/device/geolocation/geolocation_provider.h"
#include "services/device/geolocation/geolocation_provider_impl.h"
#include "services/device/geolocation/public_ip_address_geolocation_provider.h"
#include "services/device/public/mojom/battery_monitor.mojom.h"
#include "services/device/public/mojom/fingerprint.mojom.h"
#include "services/device/public/mojom/geolocation.mojom.h"
#include "services/device/public/mojom/geolocation_config.mojom.h"
#include "services/device/public/mojom/geolocation_context.mojom.h"
#include "services/device/public/mojom/geolocation_control.mojom.h"
#include "services/device/public/mojom/nfc_provider.mojom.h"
#include "services/device/public/mojom/power_monitor.mojom.h"
#include "services/device/public/mojom/screen_orientation.mojom.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "services/device/public/mojom/time_zone_monitor.mojom.h"
#include "services/device/public/mojom/vibration_manager.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "services/device/wake_lock/wake_lock_context.h"
#include "services/device/wake_lock/wake_lock_provider.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/mojom/service.mojom.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_java_ref.h"
#else
#include "services/device/public/mojom/hid.mojom.h"
#endif

#if defined(OS_CHROMEOS)
#include "services/device/media_transfer_protocol/mtp_device_manager.h"
#include "services/device/public/mojom/bluetooth_system.mojom.h"
#endif

#if defined(OS_LINUX) && defined(USE_UDEV)
#include "services/device/public/mojom/input_service.mojom.h"
#endif

namespace base {
class SingleThreadTaskRunner;
}

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace device {

#if !defined(OS_ANDROID)
class HidManagerImpl;
class SerialPortManagerImpl;
#endif

class DeviceService;
class PowerMonitorMessageBroadcaster;
class PublicIpAddressLocationNotifier;
class TimeZoneMonitor;

#if defined(OS_ANDROID)
// NOTE: See the comments on the definitions of PublicIpAddressLocationNotifier,
// |WakeLockContextCallback|, |CustomLocationProviderCallback| and
// NFCDelegate.java to understand the semantics and usage of these parameters.
std::unique_ptr<DeviceService> CreateDeviceService(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& geolocation_api_key,
    bool use_gms_core_location_provider,
    const WakeLockContextCallback& wake_lock_context_callback,
    const CustomLocationProviderCallback& custom_location_provider_callback,
    const base::android::JavaRef<jobject>& java_nfc_delegate,
    service_manager::mojom::ServiceRequest request);
#else
std::unique_ptr<DeviceService> CreateDeviceService(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& geolocation_api_key,
    const CustomLocationProviderCallback& custom_location_provider_callback,
    service_manager::mojom::ServiceRequest request);
#endif

class DeviceService : public service_manager::Service {
 public:
#if defined(OS_ANDROID)
  DeviceService(
      scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& geolocation_api_key,
      const WakeLockContextCallback& wake_lock_context_callback,
      const base::android::JavaRef<jobject>& java_nfc_delegate,
      service_manager::mojom::ServiceRequest request);
#else
  DeviceService(
      scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& geolocation_api_key,
      service_manager::mojom::ServiceRequest request);
#endif
  ~DeviceService() override;

 private:
  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  void BindFingerprintRequest(mojom::FingerprintRequest request);
  void BindGeolocationConfigRequest(mojom::GeolocationConfigRequest request);
  void BindGeolocationContextRequest(mojom::GeolocationContextRequest request);
  void BindGeolocationControlRequest(mojom::GeolocationControlRequest request);

#if defined(OS_LINUX) && defined(USE_UDEV)
  void BindInputDeviceManagerRequest(mojom::InputDeviceManagerRequest request);
#endif

#if !defined(OS_ANDROID)
  void BindBatteryMonitorRequest(mojom::BatteryMonitorRequest request);
  void BindHidManagerRequest(mojom::HidManagerRequest request);
  void BindNFCProviderRequest(mojom::NFCProviderRequest request);
  void BindVibrationManagerRequest(mojom::VibrationManagerRequest request);
#endif

#if defined(OS_CHROMEOS)
  void BindBluetoothSystemFactoryRequest(
      mojom::BluetoothSystemFactoryRequest request);
  void BindMtpManagerRequest(mojom::MtpManagerRequest request);
#endif

  void BindPowerMonitorRequest(mojom::PowerMonitorRequest request);

  void BindPublicIpAddressGeolocationProviderRequest(
      mojom::PublicIpAddressGeolocationProviderRequest request);

  void BindScreenOrientationListenerRequest(
      mojom::ScreenOrientationListenerRequest request);

  void BindSensorProviderRequest(mojom::SensorProviderRequest request);

  void BindTimeZoneMonitorRequest(mojom::TimeZoneMonitorRequest request);

  void BindWakeLockProviderRequest(mojom::WakeLockProviderRequest request);

  void BindUsbDeviceManagerRequest(mojom::UsbDeviceManagerRequest request);

  void BindUsbDeviceManagerTestRequest(
      mojom::UsbDeviceManagerTestRequest request);

  service_manager::ServiceBinding service_binding_;
  std::unique_ptr<PowerMonitorMessageBroadcaster>
      power_monitor_message_broadcaster_;
  std::unique_ptr<PublicIpAddressGeolocationProvider>
      public_ip_address_geolocation_provider_;
  std::unique_ptr<TimeZoneMonitor> time_zone_monitor_;
  std::unique_ptr<usb::DeviceManagerImpl> usb_device_manager_;
  std::unique_ptr<usb::DeviceManagerTest> usb_device_manager_test_;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  const std::string geolocation_api_key_;
  WakeLockContextCallback wake_lock_context_callback_;
  WakeLockProvider wake_lock_provider_;

#if defined(OS_ANDROID)
  // Binds |java_interface_provider_| to an interface registry that exposes
  // factories for the interfaces that are provided via Java on Android.
  service_manager::InterfaceProvider* GetJavaInterfaceProvider();

  // InterfaceProvider that is bound to the Java-side interface registry.
  service_manager::InterfaceProvider java_interface_provider_;

  bool java_interface_provider_initialized_;

  base::android::ScopedJavaGlobalRef<jobject> java_nfc_delegate_;
#else
  std::unique_ptr<HidManagerImpl> hid_manager_;
#endif

#if (defined(OS_LINUX) && defined(USE_UDEV)) || defined(OS_WIN) || \
    defined(OS_MACOSX)
  // Requests for the SerialPortManager interface must be bound to
  // |serial_port_manager_| on |serial_port_manager_task_runner_| and it will
  // be destroyed on that sequence.
  std::unique_ptr<SerialPortManagerImpl> serial_port_manager_;
  scoped_refptr<base::SequencedTaskRunner> serial_port_manager_task_runner_;
#endif

#if defined(OS_CHROMEOS)
  std::unique_ptr<MtpDeviceManager> mtp_device_manager_;
#endif

  service_manager::BinderRegistry registry_;

  DISALLOW_COPY_AND_ASSIGN(DeviceService);
};

}  // namespace device

#endif  // SERVICES_DEVICE_DEVICE_SERVICE_H_
