// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "content/browser/renderer_host/media/fake_video_capture_provider.h"
#include "content/browser/renderer_host/media/fake_video_capture_device_launcher.h"
#include "media/capture/video/fake_video_capture_device_factory.h"

namespace content {

FakeVideoCaptureProvider::FakeVideoCaptureProvider()
    : system_(std::make_unique<media::FakeVideoCaptureDeviceFactory>()) {}

FakeVideoCaptureProvider::~FakeVideoCaptureProvider() = default;

void FakeVideoCaptureProvider::GetDeviceInfosAsync(
    GetDeviceInfosCallback result_callback) {
  system_.GetDeviceInfosAsync(std::move(result_callback));
}

std::unique_ptr<VideoCaptureDeviceLauncher>
FakeVideoCaptureProvider::CreateDeviceLauncher() {
  return std::make_unique<FakeVideoCaptureDeviceLauncher>(&system_);
}

}  // namespace content
