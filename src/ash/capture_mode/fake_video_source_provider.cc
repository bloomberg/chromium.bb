// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/fake_video_source_provider.h"

#include "base/check.h"
#include "base/system/system_monitor.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/video_types.h"
#include "media/capture/video/video_capture_device_descriptor.h"
#include "media/capture/video_capture_types.h"

namespace ash {

namespace {

// Triggers a notification that video capture devices have changed.
void NotifyVideoCaptureDevicesChanged() {
  base::SystemMonitor::Get()->ProcessDevicesChanged(
      base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);
}

media::VideoCaptureFormat CreateCaptureFormat(const gfx::Size& frame_size,
                                              float frame_rate) {
  return media::VideoCaptureFormat(frame_size, frame_rate,
                                   media::PIXEL_FORMAT_I420);
}

media::VideoCaptureFormats GenerateCaptureFormatList() {
  return media::VideoCaptureFormats{
      CreateCaptureFormat(gfx::Size(160, 120), 30.f),
      CreateCaptureFormat(gfx::Size(160, 120), 20.f),
      CreateCaptureFormat(gfx::Size(176, 144), 30.f),
      CreateCaptureFormat(gfx::Size(176, 144), 24.f),
      CreateCaptureFormat(gfx::Size(320, 180), 30.f),
      CreateCaptureFormat(gfx::Size(320, 180), 24.f),
      CreateCaptureFormat(gfx::Size(320, 180), 7.f),
  };
}

media::VideoCaptureDeviceInfo CreateCaptureDeviceInfo(
    const std::string& device_id,
    const std::string& display_name,
    const std::string& model_id) {
  media::VideoCaptureDeviceInfo info{media::VideoCaptureDeviceDescriptor(
      display_name, device_id, model_id, media::VideoCaptureApi::UNKNOWN,
      media::VideoCaptureControlSupport())};
  info.supported_formats = GenerateCaptureFormatList();
  return info;
}

}  // namespace

FakeVideoSourceProvider::FakeVideoSourceProvider() = default;

FakeVideoSourceProvider::~FakeVideoSourceProvider() = default;

void FakeVideoSourceProvider::Bind(
    mojo::PendingReceiver<video_capture::mojom::VideoSourceProvider>
        pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

void FakeVideoSourceProvider::TriggerFatalErrorOnCamera(
    const std::string& device_id) {
  DCHECK(devices_map_.contains(device_id));
  devices_map_.at(device_id)->TriggerFatalError();
}

void FakeVideoSourceProvider::AddFakeCamera(const std::string& device_id,
                                            const std::string& display_name,
                                            const std::string& model_id) {
  const auto iter = devices_map_.emplace(
      device_id, std::make_unique<FakeCameraDevice>(CreateCaptureDeviceInfo(
                     device_id, display_name, model_id)));
  DCHECK(iter.second);
  NotifyVideoCaptureDevicesChanged();
}

void FakeVideoSourceProvider::RemoveFakeCamera(const std::string& device_id) {
  DCHECK(devices_map_.contains(device_id));
  devices_map_.erase(device_id);
  NotifyVideoCaptureDevicesChanged();
}

void FakeVideoSourceProvider::GetSourceInfos(GetSourceInfosCallback callback) {
  DCHECK(callback);

  std::vector<media::VideoCaptureDeviceInfo> devices;
  for (const auto& pair : devices_map_)
    devices.emplace_back(pair.second->device_info());

  // Simulate the asynchronously behavior of the actual VideoSourceProvider
  // which does a lot of asynchronous and mojo calls.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), devices));

  if (on_replied_with_source_infos_)
    std::move(on_replied_with_source_infos_).Run();
}

void FakeVideoSourceProvider::GetVideoSource(
    const std::string& source_id,
    mojo::PendingReceiver<video_capture::mojom::VideoSource> stream) {
  // The camera device may get removed after the client has issued an
  // asynchronous mojo call to `GetVideoSource()`.
  auto iter = devices_map_.find(source_id);
  if (iter == devices_map_.end())
    return;

  iter->second->Bind(std::move(stream));
}

}  // namespace ash
