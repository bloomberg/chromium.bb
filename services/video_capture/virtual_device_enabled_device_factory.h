// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_VIRTUAL_DEVICE_ENABLED_DEVICE_FACTORY_H_
#define SERVICES_VIDEO_CAPTURE_VIRTUAL_DEVICE_ENABLED_DEVICE_FACTORY_H_

#include <map>

#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/service_context_ref.h"
#include "services/video_capture/public/mojom/device.mojom.h"
#include "services/video_capture/public/mojom/device_factory.mojom.h"
#include "services/video_capture/public/mojom/virtual_device.mojom.h"

namespace video_capture {

class VirtualDeviceMojoAdapter;

// Decorator that adds support for virtual devices to a given
// mojom::DeviceFactory.
class VirtualDeviceEnabledDeviceFactory : public mojom::DeviceFactory {
 public:
  VirtualDeviceEnabledDeviceFactory(
      std::unique_ptr<service_manager::ServiceContextRef> service_ref,
      std::unique_ptr<mojom::DeviceFactory> factory);
  ~VirtualDeviceEnabledDeviceFactory() override;

  // mojom::DeviceFactory implementation.
  void GetDeviceInfos(GetDeviceInfosCallback callback) override;
  void CreateDevice(const std::string& device_id,
                    mojom::DeviceRequest device_request,
                    CreateDeviceCallback callback) override;
  void AddVirtualDevice(const media::VideoCaptureDeviceInfo& device_info,
                        mojom::ProducerPtr producer,
                        mojom::VirtualDeviceRequest virtual_device) override;

 private:
  struct VirtualDeviceEntry {
    VirtualDeviceEntry();
    ~VirtualDeviceEntry();
    VirtualDeviceEntry(VirtualDeviceEntry&& other);
    VirtualDeviceEntry& operator=(VirtualDeviceEntry&& other);

    std::unique_ptr<VirtualDeviceMojoAdapter> device;
    std::unique_ptr<mojo::Binding<mojom::VirtualDevice>> producer_binding;
    std::unique_ptr<mojo::Binding<mojom::Device>> consumer_binding;
  };

  void OnGetDeviceInfos(
      GetDeviceInfosCallback callback,
      const std::vector<media::VideoCaptureDeviceInfo>& device_infos);

  void OnVirtualDeviceProducerConnectionErrorOrClose(
      const std::string& device_id);
  void OnVirtualDeviceConsumerConnectionErrorOrClose(
      const std::string& device_id);

  std::map<std::string, VirtualDeviceEntry> virtual_devices_by_id_;
  const std::unique_ptr<service_manager::ServiceContextRef> service_ref_;
  const std::unique_ptr<mojom::DeviceFactory> device_factory_;

  base::WeakPtrFactory<VirtualDeviceEnabledDeviceFactory> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(VirtualDeviceEnabledDeviceFactory);
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_VIRTUAL_DEVICE_ENABLED_DEVICE_FACTORY_H_
