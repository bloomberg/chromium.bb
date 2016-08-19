// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_FACTORY_IMPL_H_
#define SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_FACTORY_IMPL_H_

#include <vector>

#include "services/video_capture/public/interfaces/video_capture_device_factory.mojom.h"
#include "services/video_capture/video_capture_device_impl.h"

namespace video_capture {

class VideoCaptureDeviceFactoryImpl : public mojom::VideoCaptureDeviceFactory {
 public:
  VideoCaptureDeviceFactoryImpl();
  ~VideoCaptureDeviceFactoryImpl() override;

  void AddDevice(mojom::VideoCaptureDeviceDescriptorPtr descriptor,
                 std::unique_ptr<VideoCaptureDeviceImpl> device);

  // mojom::VideoCaptureDeviceFactory:
  void EnumerateDeviceDescriptors(
      const EnumerateDeviceDescriptorsCallback& callback) override;
  void GetSupportedFormats(
      mojom::VideoCaptureDeviceDescriptorPtr device_descriptor,
      const GetSupportedFormatsCallback& callback) override;
  void CreateDevice(mojom::VideoCaptureDeviceDescriptorPtr device_descriptor,
                    mojom::VideoCaptureDeviceRequest device_request,
                    const CreateDeviceCallback& callback) override;

 private:
  // We use a std::vector of structs with the |descriptor| field as a unique
  // keys. We do it this way instead of using a std::map because Mojo-generated
  // structs (here VideoCaptureDeviceDescriptor) do not (yet) generate a
  // comparison operator, and we do not want to provide a custom one. The
  // performance penalty of linear-time lookup should be minimal assuming that
  // the number of capture devices is typically small.
  class DeviceEntry {
   public:
    DeviceEntry(mojom::VideoCaptureDeviceDescriptorPtr descriptor,
                std::unique_ptr<VideoCaptureDeviceImpl> bindable_target);
    ~DeviceEntry();
    DeviceEntry(DeviceEntry&& other);
    DeviceEntry& operator=(DeviceEntry&& other);

    mojom::VideoCaptureDeviceDescriptorPtr MakeDescriptorCopy() const;

   private:
    mojom::VideoCaptureDeviceDescriptorPtr descriptor_;
    std::unique_ptr<VideoCaptureDeviceImpl> device_;

    DISALLOW_COPY_AND_ASSIGN(DeviceEntry);
  };

  std::vector<DeviceEntry> devices_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_FACTORY_IMPL_H_
