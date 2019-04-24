// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/cros_image_capture_impl.h"

#include <utility>
#include <vector>

#include "base/task/post_task.h"

namespace media {

CrosImageCaptureImpl::CrosImageCaptureImpl(ReprocessManager* reprocess_manager)
    : reprocess_manager_(reprocess_manager) {}

CrosImageCaptureImpl::~CrosImageCaptureImpl() = default;

void CrosImageCaptureImpl::BindRequest(
    cros::mojom::CrosImageCaptureRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void CrosImageCaptureImpl::GetSupportedEffects(
    GetSupportedEffectsCallback callback) {
  reprocess_manager_->GetSupportedEffects(
      base::BindOnce(&CrosImageCaptureImpl::OnGetSupportedEffects,
                     base::Unretained(this), std::move(callback)));
}

void CrosImageCaptureImpl::SetReprocessOption(
    cros::mojom::Effect effect,
    SetReprocessOptionCallback callback) {
  reprocess_manager_->SetReprocessOption(effect, std::move(callback));
}

void CrosImageCaptureImpl::OnGetSupportedEffects(
    GetSupportedEffectsCallback callback,
    base::flat_set<cros::mojom::Effect> supported_effects) {
  std::vector<cros::mojom::Effect> effects(supported_effects.begin(),
                                           supported_effects.end());
  std::move(callback).Run(std::move(effects));
}

}  // namespace media
