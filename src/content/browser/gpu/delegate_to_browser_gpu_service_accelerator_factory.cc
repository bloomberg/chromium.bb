// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/delegate_to_browser_gpu_service_accelerator_factory.h"

#include "content/browser/gpu/video_capture_dependencies.h"

namespace content {

void DelegateToBrowserGpuServiceAcceleratorFactory::CreateJpegDecodeAccelerator(
    media::mojom::JpegDecodeAcceleratorRequest jda_request) {
  VideoCaptureDependencies::CreateJpegDecodeAccelerator(std::move(jda_request));
}

}  // namespace content
