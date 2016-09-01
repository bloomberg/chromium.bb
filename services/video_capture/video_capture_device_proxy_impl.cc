// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "services/video_capture/device_client_mojo_to_media_adapter.h"
#include "services/video_capture/video_capture_device_proxy_impl.h"

namespace video_capture {

VideoCaptureDeviceProxyImpl::VideoCaptureDeviceProxyImpl(
    std::unique_ptr<media::VideoCaptureDevice> device)
    : device_(std::move(device)) {}

VideoCaptureDeviceProxyImpl::~VideoCaptureDeviceProxyImpl() = default;

void VideoCaptureDeviceProxyImpl::Start(
    mojom::VideoCaptureFormatPtr requested_format,
    mojom::ResolutionChangePolicy resolution_change_policy,
    mojom::PowerLineFrequency power_line_frequency,
    mojom::VideoCaptureDeviceClientPtr client) {
  media::VideoCaptureParams params;
  params.requested_format = ConvertFromMojoToMedia(std::move(requested_format));
  params.resolution_change_policy =
      ConvertFromMojoToMedia(resolution_change_policy);
  params.power_line_frequency = ConvertFromMojoToMedia(power_line_frequency);
  auto media_client =
      base::WrapUnique(new DeviceClientMojoToMediaAdapter(std::move(client)));
  device_->AllocateAndStart(params, std::move(media_client));
}

// static
media::VideoCaptureFormat VideoCaptureDeviceProxyImpl::ConvertFromMojoToMedia(
    mojom::VideoCaptureFormatPtr format) {
  media::VideoCaptureFormat result;
  result.pixel_format = ConvertFromMojoToMedia(format->pixel_format);
  result.pixel_storage = ConvertFromMojoToMedia(format->pixel_storage);
  result.frame_size.SetSize(format->frame_size.width(),
                            format->frame_size.height());
  result.frame_rate = format->frame_rate;
  return result;
}

// static
media::VideoPixelFormat VideoCaptureDeviceProxyImpl::ConvertFromMojoToMedia(
    media::mojom::VideoFormat format) {
  // Since there are static_asserts in place in
  // media/mojo/common/media_type_converters.cc to guarantee equality of the
  // underlying representations, we can simply static_cast to convert.
  return static_cast<media::VideoPixelFormat>(format);
}

// static
media::VideoPixelStorage VideoCaptureDeviceProxyImpl::ConvertFromMojoToMedia(
    mojom::VideoPixelStorage storage) {
  switch (storage) {
    case mojom::VideoPixelStorage::CPU:
      return media::PIXEL_STORAGE_CPU;
    case mojom::VideoPixelStorage::GPUMEMORYBUFFER:
      return media::PIXEL_STORAGE_GPUMEMORYBUFFER;
  }
  NOTREACHED();
  return media::PIXEL_STORAGE_CPU;
}

// static
media::ResolutionChangePolicy
VideoCaptureDeviceProxyImpl::ConvertFromMojoToMedia(
    mojom::ResolutionChangePolicy policy) {
  switch (policy) {
    case mojom::ResolutionChangePolicy::FIXED_RESOLUTION:
      return media::RESOLUTION_POLICY_FIXED_RESOLUTION;
    case mojom::ResolutionChangePolicy::FIXED_ASPECT_RATIO:
      return media::RESOLUTION_POLICY_FIXED_ASPECT_RATIO;
    case mojom::ResolutionChangePolicy::ANY_WITHIN_LIMIT:
      return media::RESOLUTION_POLICY_ANY_WITHIN_LIMIT;
  }
  NOTREACHED();
  return media::RESOLUTION_POLICY_FIXED_RESOLUTION;
}

// static
media::PowerLineFrequency VideoCaptureDeviceProxyImpl::ConvertFromMojoToMedia(
    mojom::PowerLineFrequency frequency) {
  switch (frequency) {
    case mojom::PowerLineFrequency::DEFAULT:
      return media::PowerLineFrequency::FREQUENCY_DEFAULT;
    case mojom::PowerLineFrequency::HZ_50:
      return media::PowerLineFrequency::FREQUENCY_50HZ;
    case mojom::PowerLineFrequency::HZ_60:
      return media::PowerLineFrequency::FREQUENCY_60HZ;
  }
  NOTREACHED();
  return media::PowerLineFrequency::FREQUENCY_DEFAULT;
}

}  // namespace video_capture
