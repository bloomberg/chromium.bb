// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_util.h"

#include <math.h>

#include "base/check_op.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/transform.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/view.h"
#include "ui/views/views_features.h"

namespace views {

gfx::Transform GetTransformSubpixelCorrection(const gfx::Transform& transform,
                                              float device_scale_factor) {
  gfx::Point3F origin;
  transform.TransformPoint(&origin);

  const gfx::Vector2dF offset_in_dip = origin.AsPointF().OffsetFromOrigin();

  // Scale the origin to screen space
  origin.Scale(device_scale_factor);

  // Compute the rounded offset in screen space and finally unscale it back to
  // DIP space.
  gfx::Vector2dF aligned_offset_in_dip = origin.AsPointF().OffsetFromOrigin();
  aligned_offset_in_dip.set_x(gfx::ToRoundedInt(aligned_offset_in_dip.x()));
  aligned_offset_in_dip.set_y(gfx::ToRoundedInt(aligned_offset_in_dip.y()));
  aligned_offset_in_dip.Scale(1.f / device_scale_factor);

  // Compute the subpixel offset correction and apply it to the transform.
  gfx::Transform subpixel_correction;
  subpixel_correction.Translate(aligned_offset_in_dip - offset_in_dip);
#if DCHECK_IS_ON()
  const float kEpsilon = 0.0001f;
  gfx::Point3F offset;

  gfx::Transform transform_corrected(transform);
  transform_corrected.ConcatTransform(subpixel_correction);
  transform_corrected.TransformPoint(&offset);
  offset.Scale(device_scale_factor);

  if (!std::isnan(offset.x()))
    DCHECK_LT(std::abs(gfx::ToRoundedInt(offset.x()) - offset.x()), kEpsilon);
  if (!std::isnan(offset.y()))
    DCHECK_LT(std::abs(gfx::ToRoundedInt(offset.y()) - offset.y()), kEpsilon);
#endif
  return subpixel_correction;
}

bool UsingPlatformHighContrastInkDrop(const View* view) {
  const ui::NativeTheme* theme = view->GetNativeTheme();
  return theme &&
         theme->GetDefaultSystemColorScheme() ==
             ui::NativeTheme::ColorScheme::kPlatformHighContrast &&
         base::FeatureList::IsEnabled(
             features::kEnablePlatformHighContrastInkDrop);
}

}  // namespace views
