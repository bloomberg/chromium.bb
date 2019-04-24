// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_CROS_IMAGE_CAPTURE_IMPL_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_CROS_IMAGE_CAPTURE_IMPL_H_

#include "base/containers/flat_set.h"
#include "media/capture/video/chromeos/mojo/cros_image_capture.mojom.h"
#include "media/capture/video/chromeos/reprocess_manager.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace media {

class CrosImageCaptureImpl : public cros::mojom::CrosImageCapture {
 public:
  explicit CrosImageCaptureImpl(ReprocessManager* reprocess_manager);
  ~CrosImageCaptureImpl() override;

  void BindRequest(cros::mojom::CrosImageCaptureRequest request);

  // cros::mojom::CrosImageCapture implementations.

  void GetSupportedEffects(GetSupportedEffectsCallback callback) override;
  void SetReprocessOption(cros::mojom::Effect effect,
                          SetReprocessOptionCallback callback) override;

 private:
  void OnGetSupportedEffects(
      GetSupportedEffectsCallback callback,
      base::flat_set<cros::mojom::Effect> supported_effects);

  ReprocessManager* reprocess_manager_;  // weak

  mojo::BindingSet<cros::mojom::CrosImageCapture> bindings_;

  DISALLOW_COPY_AND_ASSIGN(CrosImageCaptureImpl);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_CROS_IMAGE_CAPTURE_IMPL_H_
