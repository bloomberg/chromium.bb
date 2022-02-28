// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/image_model_utils.h"

#include "ui/native_theme/themed_vector_icon.h"

namespace views {

gfx::ImageSkia GetImageSkiaFromImageModel(
    const ui::ImageModel& model,
    const ui::ColorProvider* color_provider) {
  if (model.IsImage())
    return model.GetImage().AsImageSkia();

  if (model.IsVectorIcon()) {
    DCHECK(color_provider);
    return ui::ThemedVectorIcon(model.GetVectorIcon())
        .GetImageSkia(color_provider);
  }

  if (model.IsImageGenerator())
    return model.GetImageGenerator().Run(color_provider);

  return gfx::ImageSkia();
}

}  // namespace views
