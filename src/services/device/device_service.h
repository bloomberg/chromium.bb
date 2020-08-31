// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_DEVICE_SERVICE_H_
#define SERVICES_DEVICE_DEVICE_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/device/geolocation/geolocation_provider.h"
#include "services/device/geolocation/geolocation_provider_impl.h"
#include "services/device/geolocation/public_ip_address_geolocation_provider.h"
#include "services/device/public/mojom/battery_monitor.mojom.h"
#include "services/device/public/mojom/device_service.mojom.h"
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
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "services/device/public/mojom/usb_manager_test.mojom.h"
#include "services/device/public/mojom/vibration_manager.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "services/device/usb/mojo/device_manager_impl.h"
#include "services/device/usb/mojo/device_manager_test.h"
#include "services/device/wake_lock/wake_lock_context.h"
#include "services/device/wake_lock/wake_lock_provider.h"
#include "services/service_manager/public/cpp/interface_provider.h"

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
class NetworkConnectionTracker;
class SharedURLLoaderFactory;
}  // namespace network

namespace device {

#if !defined(OS_ANDROID)
class HidManagerImpl;
class SerialPortManagerImpl;
#endif

class DeviceService;
class PlatformSensorProvider;
class PowerMonitorMessageBroadcaster;
class PublicIpAddressLocationNotifier;
class SensorProviderImpl;
class TimeZoneMonitor;

#if defined(OS_ANDROID)
// NOTE: See the comments on the definitions of PublicIpAddressLocationNotifier,
// |WakeLockContextCallback|, |CustomLocationProviderCallback| and
// NFCDelegate.java to understand the semantics and usage of these parameters.
std::unique_ptr<DeviceService> CreateDeviceService(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    network::NetworkConnectionTracker* network_connection_tracker,
    const std::string& geolocation_api_key,
    bool use_gms_core_location_provider,
    const WakeLockContextCallback& wake_lock_context_callback,
    const CustomLocationProviderCallback& custom_location_provider_callback,
    const base::android::JavaRef<jobject>& java_nfc_delegate,
    mojo::PendingReceiver<mojom::DeviceService> receiver);
#else
std::unique_ptr<DeviceService> CreateDeviceService(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    network::NetworkConnectionTracker* network_connection_tracker,
    const std::string& geolocation_api_key,
    const CustomLocationProviderCallback& custom_location_provider_callback,
    mojo::PendingReceiver<mojom::DeviceService> receiver);
#endif

class DeviceService : public mojom::DeviceService {
 public:
#if defined(OS_ANDROID)
  DeviceService(
      scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      network::NetworkConnectionTracker* network_connection_tracker,
      const std::string& geolocation_api_key,
      const WakeLockContextCallback& wake_lock_context_callback,
      const base::android::JavaRef<jobject>& java_nfc_delegate,
      mojo::PendingReceiver<mojom::DeviceService> receiver);
#else
  DeviceService(
      scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      network::NetworkConnectionTracker* network_connection_tracker,
      const std::string& geolocation_api_key,
      mojo::PendingReceiver<mojom::DeviceService> receiver);
#endif
  ~DeviceService() override;

  void AddReceiver(mojo::PendingReceiver<mojom::DeviceService> receiver);

  void SetPlatformSensorProviderForTesting(
      std::unique_ptr<PlatformSensorProvider> provider);

  // Supports global override of GeolocationContext binding within the service.
  using GeolocationContextBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<mojom::GeolocationContext>)>;
  static void OverrideGeolocationContextBinderForTesting(
      GeolocationContextBinder binder);

 private:
  // mojom::DeviceService implementation:
  void BindFingerprint(
      mojo::PendingReceiver<mojom::Fingerprint> receiver) override;
  void BindGeolocationConfig(
      mojo::PendingReceiver<mojom::GeolocationConfig> receiver) override;
  void BindGeolocationContext(
      mojo::PendingReceiver<mojom::GeolocationContext> receiver) override;
  void BindGeolocationControl(
      mojo::PendingReceiver<mojom::GeolocationControl> receiver) override;

#if defined(OS_LINUX) && defined(USE_UDEV)
  void BindInputDeviceManager(
      mojo::PendingReceiver<mojom::InputDeviceManager> receiver) override;
#endif

  void BindBatteryMonitor(
      mojo::PendingReceiver<mojom::BatteryMonitor> receiver) override;
  void BindNFCProvider(
      mojo::PendingReceiver<mojom::NFCProvider> receiver) override;
  void BindVibrationManager(
      mojo::PendingReceiver<mojom::VibrationManager> receiver) override;

#if !defined(OS_ANDROID)
  void BindHidManager(
      mojo::PendingReceiver<mojom::HidManager> receiver) override;
#endif

#if defined(OS_CHROMEOS)
  void BindBluetoothSystemFactory(
      mojo::PendingReceiver<mojom::BluetoothSystemFactory> receiver) override;
  void BindMtpManager(
      mojo::PendingReceiver<mojom::MtpManager> receiver) override;
#endif

  void BindPowerMonitor(
      mojo::PendingReceiver<mojom::PowerMonitor> receiver) override;

  void BindPublicIpAddressGeolocationProvider(
      mojo::PendingReceiver<mojom::PublicIpAddressGeolocationProvider> receiver)
      override;

  void BindScreenOrientationListener(
      mojo::PendingReceiver<mojom::ScreenOrientationListener> receiver)
      override;

  void BindSensorProvider(
      mojo::PendingReceiver<mojom::SensorProvider> receiver) override;

  void BindSerialPortManager(
      mojo::PendingReceiver<mojom::SerialPortManager> receiver) override;

  void BindTimeZoneMonitor(
      mojo::PendingReceiver<mojom::TimeZoneMonitor> receiver) override;

  void BindWakeLockProvider(
      mojo::PendingReceiver<mojom::WakeLockProvider> receiver) override;

  void BindUsbDeviceManager(
      mojo::PendingReceiver<mojom::UsbDeviceManager> receiver) override;

  void BindUsbDeviceManagerTest(
      mojo::PendingReceiver<mojom::UsbDeviceManagerTest> receiver) override;

  mojo::ReceiverSet<mojom::DeviceService> receivers_;
  std::unique_ptr<PowerMonitorMessageBroadcaster>
      power_monitor_message_broadcaster_;
  std::unique_ptr<PublicIpAddressGeolocationProvider>
      public_ip_address_geolocation_provider_;
  std::unique_ptr<SensorProviderImpl> sensor_provider_;
  std::unique_ptr<TimeZoneMonitor> time_zone_monitor_;
  std::unique_ptr<usb::DeviceManagerImpl> usb_device_manager_;
  std::unique_ptr<usb::DeviceManagerTest> usb_device_manager_test_;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  network::NetworkConnectionTracker* network_connection_tracker_;

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

  DISALLOW_COPY_AND_ASSIGN(DeviceService);
};

}  // namespace device

#endif  // SERVICES_DEVICE_DEVICE_SERVICE_H_
