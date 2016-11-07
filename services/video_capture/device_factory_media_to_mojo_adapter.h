// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_DEVICE_FACTORY_MEDIA_TO_MOJO_ADAPTER_H_
#define SERVICES_VIDEO_CAPTURE_DEVICE_FACTORY_MEDIA_TO_MOJO_ADAPTER_H_

#include <map>

#include "media/capture/video/video_capture_device_client.h"
#include "media/capture/video/video_capture_device_factory.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/video_capture/public/interfaces/video_capture_device_factory.mojom.h"

namespace video_capture {

class VideoCaptureDeviceProxyImpl;

// Wraps a media::VideoCaptureDeviceFactory and exposes its functionality
// through the mojom::VideoCaptureDeviceFactory interface.
// Keeps track of device instances that have been created to ensure that
// it does not create more than one instance of the same
// media::VideoCaptureDevice at the same time.
class DeviceFactoryMediaToMojoAdapter
    : public mojom::VideoCaptureDeviceFactory {
 public:
  DeviceFactoryMediaToMojoAdapter(
      std::unique_ptr<media::VideoCaptureDeviceFactory> device_factory,
      const media::VideoCaptureJpegDecoderFactoryCB&
          jpeg_decoder_factory_callback);
  ~DeviceFactoryMediaToMojoAdapter() override;

  // mojom::VideoCaptureDeviceFactory:
  void EnumerateDeviceDescriptors(
      const EnumerateDeviceDescriptorsCallback& callback) override;
  void GetSupportedFormats(
      const media::VideoCaptureDeviceDescriptor& device_descriptor,
      const GetSupportedFormatsCallback& callback) override;
  void CreateDeviceProxy(
      const media::VideoCaptureDeviceDescriptor& device_descriptor,
      mojom::VideoCaptureDeviceProxyRequest proxy_request,
      const CreateDeviceProxyCallback& callback) override;

 private:
  struct ActiveDeviceEntry {
    ActiveDeviceEntry();
    ~ActiveDeviceEntry();
    ActiveDeviceEntry(ActiveDeviceEntry&& other);
    ActiveDeviceEntry& operator=(ActiveDeviceEntry&& other);

    std::unique_ptr<VideoCaptureDeviceProxyImpl> device_proxy;
    // TODO(chfremer) Use mojo::Binding<> directly instead of unique_ptr<> when
    // mojo::Binding<> supports move operators.
    // https://crbug.com/644314
    std::unique_ptr<mojo::Binding<mojom::VideoCaptureDeviceProxy>> binding;
  };

  void OnClientConnectionErrorOrClose(
      const media::VideoCaptureDeviceDescriptor& descriptor);

  const std::unique_ptr<media::VideoCaptureDeviceFactory> device_factory_;
  const media::VideoCaptureJpegDecoderFactoryCB jpeg_decoder_factory_callback_;
  std::map<media::VideoCaptureDeviceDescriptor, ActiveDeviceEntry>
      active_devices_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_DEVICE_FACTORY_MEDIA_TO_MOJO_ADAPTER_H_
