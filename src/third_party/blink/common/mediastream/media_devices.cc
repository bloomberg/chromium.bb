// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/mediastream/media_devices.h"
#include "media/capture/video/video_capture_device_descriptor.h"

namespace blink {

WebMediaDeviceInfo::WebMediaDeviceInfo()
    : video_facing(media::VideoFacingMode::MEDIA_VIDEO_FACING_NONE) {}

WebMediaDeviceInfo::WebMediaDeviceInfo(const WebMediaDeviceInfo& other) =
    default;

WebMediaDeviceInfo::WebMediaDeviceInfo(WebMediaDeviceInfo&& other) = default;

WebMediaDeviceInfo::WebMediaDeviceInfo(const std::string& device_id,
                                       const std::string& label,
                                       const std::string& group_id,
                                       media::VideoFacingMode video_facing)
    : device_id(device_id),
      label(label),
      group_id(group_id),
      video_facing(video_facing) {}

WebMediaDeviceInfo::WebMediaDeviceInfo(
    const media::VideoCaptureDeviceDescriptor& descriptor)
    : device_id(descriptor.device_id),
      label(descriptor.GetNameAndModel()),
      video_facing(descriptor.facing) {}

WebMediaDeviceInfo::~WebMediaDeviceInfo() = default;

WebMediaDeviceInfo& WebMediaDeviceInfo::operator=(
    const WebMediaDeviceInfo& other) = default;

WebMediaDeviceInfo& WebMediaDeviceInfo::operator=(WebMediaDeviceInfo&& other) =
    default;

bool operator==(const WebMediaDeviceInfo& first,
                const WebMediaDeviceInfo& second) {
  // Do not use the |group_id| and |video_facing| fields for equality comparison
  // since they are currently not fully supported by the video-capture layer.
  // The modification of those fields by heuristics in upper layers does not
  // result in a different device.
  return first.device_id == second.device_id && first.label == second.label;
}

}  // namespace blink
