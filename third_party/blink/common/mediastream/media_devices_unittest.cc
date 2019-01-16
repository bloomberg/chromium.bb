// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/mediastream/media_devices.h"
#include "media/audio/audio_device_description.h"
#include "media/capture/video/video_capture_device_descriptor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(MediaDevicesTest, MediaDeviceInfoFromVideoDescriptor) {
  media::VideoCaptureDeviceDescriptor descriptor(
      "display_name", "device_id", "model_id", media::VideoCaptureApi::UNKNOWN);

  // TODO(guidou): Add test for group ID when supported. See crbug.com/627793.
  WebMediaDeviceInfo device_info(descriptor);
  EXPECT_EQ(descriptor.device_id, device_info.device_id);
  EXPECT_EQ(descriptor.GetNameAndModel(), device_info.label);
}

}  // namespace blink
