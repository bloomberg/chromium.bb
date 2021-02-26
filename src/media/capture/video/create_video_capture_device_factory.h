// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CREATE_VIDEO_CAPTURE_DEVICE_FACTORY_H_
#define MEDIA_CAPTURE_VIDEO_CREATE_VIDEO_CAPTURE_DEVICE_FACTORY_H_

#include <memory>

#include "base/single_thread_task_runner.h"
#include "build/chromeos_buildflags.h"
#include "media/capture/capture_export.h"
#include "media/capture/video/video_capture_device_factory.h"

namespace media {

class CameraAppDeviceBridgeImpl;

std::unique_ptr<VideoCaptureDeviceFactory> CAPTURE_EXPORT
CreateVideoCaptureDeviceFactory(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

#if BUILDFLAG(IS_ASH)
std::unique_ptr<VideoCaptureDeviceFactory> CAPTURE_EXPORT
CreateVideoCaptureDeviceFactory(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    media::CameraAppDeviceBridgeImpl* camera_app_device_bridge);
#endif  // BUILDFLAG(IS_ASH)

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CREATE_VIDEO_CAPTURE_DEVICE_FACTORY_H_
