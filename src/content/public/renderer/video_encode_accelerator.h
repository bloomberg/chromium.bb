// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_VIDEO_ENCODE_ACCELERATOR_H_
#define CONTENT_PUBLIC_RENDERER_VIDEO_ENCODE_ACCELERATOR_H_

#include <memory>

#include "base/callback.h"
#include "base/single_thread_task_runner.h"
#include "content/common/content_export.h"
#include "media/video/video_encode_accelerator.h"

namespace content {

// Called when CreateVideoEncodeAccelerator request is complete.
// The |vea| object must be accessed on the thread associated with the
// |encode_task_runner|.
typedef base::Callback<void(
    scoped_refptr<base::SingleThreadTaskRunner> encoder_task_runner,
    std::unique_ptr<media::VideoEncodeAccelerator> vea)>
    OnCreateVideoEncodeAcceleratorCallback;

// Generates an instance of media::VideoEncodeAccelerator.
CONTENT_EXPORT void
CreateVideoEncodeAccelerator(
    const OnCreateVideoEncodeAcceleratorCallback& callback);

// Returns list of encoding profiles supported by VideoEncodeAccelerator.
CONTENT_EXPORT media::VideoEncodeAccelerator::SupportedProfiles
    GetSupportedVideoEncodeAcceleratorProfiles();

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_VIDEO_ENCODE_ACCELERATOR_H_
