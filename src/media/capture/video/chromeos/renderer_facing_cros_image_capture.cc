// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/renderer_facing_cros_image_capture.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/task/post_task.h"
#include "media/base/bind_to_current_loop.h"

namespace media {

RendererFacingCrosImageCapture::RendererFacingCrosImageCapture(
    cros::mojom::CrosImageCapturePtr api_ptr,
    DeviceIdMappingCallback mapping_callback)
    : cros_image_capture_(std::move(api_ptr)),
      mapping_callback_(std::move(mapping_callback)),
      weak_ptr_factory_(this) {}

RendererFacingCrosImageCapture::~RendererFacingCrosImageCapture() = default;

void RendererFacingCrosImageCapture::GetCameraInfoWithRealId(
    GetCameraInfoCallback callback,
    const base::Optional<std::string>& device_id) {
  DCHECK(device_id.has_value());
  cros_image_capture_->GetCameraInfo(*device_id, std::move(callback));
}

void RendererFacingCrosImageCapture::SetReprocessOptionWithRealId(
    cros::mojom::Effect effect,
    SetReprocessOptionCallback callback,
    const base::Optional<std::string>& device_id) {
  DCHECK(device_id.has_value());
  cros_image_capture_->SetReprocessOption(*device_id, effect,
                                          std::move(callback));
}

void RendererFacingCrosImageCapture::GetCameraInfo(
    const std::string& source_id,
    GetCameraInfoCallback callback) {
  mapping_callback_.Run(
      source_id, media::BindToCurrentLoop(base::BindOnce(
                     &RendererFacingCrosImageCapture::GetCameraInfoWithRealId,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
}

void RendererFacingCrosImageCapture::SetReprocessOption(
    const std::string& source_id,
    cros::mojom::Effect effect,
    SetReprocessOptionCallback callback) {
  mapping_callback_.Run(
      source_id,
      media::BindToCurrentLoop(base::BindOnce(
          &RendererFacingCrosImageCapture::SetReprocessOptionWithRealId,
          weak_ptr_factory_.GetWeakPtr(), effect, std::move(callback))));
}

}  // namespace media