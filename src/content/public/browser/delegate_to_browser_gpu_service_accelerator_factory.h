// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DELEGATE_TO_BROWSER_GPU_SERVICE_ACCELERATOR_FACTORY_H_
#define CONTENT_PUBLIC_BROWSER_DELEGATE_TO_BROWSER_GPU_SERVICE_ACCELERATOR_FACTORY_H_

#include "content/common/content_export.h"
#include "services/video_capture/public/mojom/device_factory_provider.mojom.h"

namespace content {

// Implementation of video_capture::mojom::AcceleratorFactor that satisfies
// requests for a JpegDecodeAccelerator by delegating to the global instance of
// viz::mojom::GpuService that is accessible from the Browser process.
class CONTENT_EXPORT DelegateToBrowserGpuServiceAcceleratorFactory
    : public video_capture::mojom::AcceleratorFactory {
 public:
  void CreateJpegDecodeAccelerator(
      media::mojom::JpegDecodeAcceleratorRequest jda_request) override;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DELEGATE_TO_BROWSER_GPU_SERVICE_ACCELERATOR_FACTORY_H_
