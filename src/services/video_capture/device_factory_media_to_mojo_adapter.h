// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_DEVICE_FACTORY_MEDIA_TO_MOJO_ADAPTER_H_
#define SERVICES_VIDEO_CAPTURE_DEVICE_FACTORY_MEDIA_TO_MOJO_ADAPTER_H_

#include <map>

#include "media/capture/video/video_capture_device_client.h"
#include "media/capture/video/video_capture_system.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/service_context_ref.h"
#include "services/video_capture/device_factory.h"
#include "services/video_capture/public/mojom/devices_changed_observer.mojom.h"

#if defined(OS_CHROMEOS)
#include "media/capture/video/chromeos/mojo/cros_image_capture.mojom.h"
#endif  // defined(OS_CHROMEOS)

namespace video_capture {

class DeviceMediaToMojoAdapter;

// Wraps a media::VideoCaptureSystem and exposes its functionality through the
// mojom::DeviceFactory interface. Keeps track of device instances that have
// been created to ensure that it does not create more than one instance of the
// same media::VideoCaptureDevice at the same time.
class DeviceFactoryMediaToMojoAdapter : public DeviceFactory {
 public:
  DeviceFactoryMediaToMojoAdapter(
      std::unique_ptr<media::VideoCaptureSystem> capture_system,
      media::MojoJpegDecodeAcceleratorFactoryCB jpeg_decoder_factory_callback,
      scoped_refptr<base::SequencedTaskRunner> jpeg_decoder_task_runner);
  ~DeviceFactoryMediaToMojoAdapter() override;

  // DeviceFactory implementation.
  void SetServiceRef(
      std::unique_ptr<service_manager::ServiceContextRef> service_ref) override;
  void GetDeviceInfos(GetDeviceInfosCallback callback) override;
  void CreateDevice(const std::string& device_id,
                    mojom::DeviceRequest device_request,
                    CreateDeviceCallback callback) override;
  void AddSharedMemoryVirtualDevice(
      const media::VideoCaptureDeviceInfo& device_info,
      mojom::ProducerPtr producer,
      bool send_buffer_handles_to_producer_as_raw_file_descriptors,
      mojom::SharedMemoryVirtualDeviceRequest virtual_device) override;
  void AddTextureVirtualDevice(
      const media::VideoCaptureDeviceInfo& device_info,
      mojom::TextureVirtualDeviceRequest virtual_device) override;
  void RegisterVirtualDevicesChangedObserver(
      mojom::DevicesChangedObserverPtr observer,
      bool raise_event_if_virtual_devices_already_present) override;

#if defined(OS_CHROMEOS)
  void BindCrosImageCaptureRequest(
      cros::mojom::CrosImageCaptureRequest request) override;
#endif  // defined(OS_CHROMEOS)

 private:
  struct ActiveDeviceEntry {
    ActiveDeviceEntry();
    ~ActiveDeviceEntry();
    ActiveDeviceEntry(ActiveDeviceEntry&& other);
    ActiveDeviceEntry& operator=(ActiveDeviceEntry&& other);

    std::unique_ptr<DeviceMediaToMojoAdapter> device;
    // TODO(chfremer) Use mojo::Binding<> directly instead of unique_ptr<> when
    // mojo::Binding<> supports move operators.
    // https://crbug.com/644314
    std::unique_ptr<mojo::Binding<mojom::Device>> binding;
  };

  void CreateAndAddNewDevice(const std::string& device_id,
                             mojom::DeviceRequest device_request,
                             CreateDeviceCallback callback);
  void OnClientConnectionErrorOrClose(const std::string& device_id);

  std::unique_ptr<service_manager::ServiceContextRef> service_ref_;
  const std::unique_ptr<media::VideoCaptureSystem> capture_system_;
  const media::MojoJpegDecodeAcceleratorFactoryCB
      jpeg_decoder_factory_callback_;
  scoped_refptr<base::SequencedTaskRunner> jpeg_decoder_task_runner_;
  std::map<std::string, ActiveDeviceEntry> active_devices_by_id_;
  bool has_called_get_device_infos_;

  base::WeakPtrFactory<DeviceFactoryMediaToMojoAdapter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeviceFactoryMediaToMojoAdapter);
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_DEVICE_FACTORY_MEDIA_TO_MOJO_ADAPTER_H_
