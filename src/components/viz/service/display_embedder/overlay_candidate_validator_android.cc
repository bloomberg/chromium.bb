// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/overlay_candidate_validator_android.h"

#include "cc/base/math_util.h"
#include "components/viz/service/display/overlay_strategy_fullscreen.h"
#include "components/viz/service/display/overlay_strategy_single_on_top.h"
#include "components/viz/service/display/overlay_strategy_underlay.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gl/android/android_surface_control_compat.h"

namespace viz {
namespace {

gfx::RectF ClipFromOrigin(gfx::RectF input) {
  if (input.x() < 0.f) {
    input.set_width(input.width() + input.x());
    input.set_x(0.f);
  }

  if (input.y() < 0) {
    input.set_height(input.height() + input.y());
    input.set_y(0.f);
  }

  return input;
}

}  // namespace

OverlayCandidateValidatorAndroid::OverlayCandidateValidatorAndroid() = default;
OverlayCandidateValidatorAndroid::~OverlayCandidateValidatorAndroid() = default;

void OverlayCandidateValidatorAndroid::GetStrategies(
    OverlayProcessor::StrategyList* strategies) {
  // Added in priority order, most to least desirable.
  strategies->push_back(std::make_unique<OverlayStrategyFullscreen>(this));
  strategies->push_back(std::make_unique<OverlayStrategySingleOnTop>(this));
  strategies->push_back(std::make_unique<OverlayStrategyUnderlay>(
      this, OverlayStrategyUnderlay::OpaqueMode::AllowTransparentCandidates));
}

bool OverlayCandidateValidatorAndroid::AllowCALayerOverlays() {
  return false;
}

bool OverlayCandidateValidatorAndroid::AllowDCLayerOverlays() {
  return false;
}

void OverlayCandidateValidatorAndroid::CheckOverlaySupport(
    OverlayCandidateList* surfaces) {
  DCHECK(!surfaces->empty());

  // Only update the last candidate that was added to the list. All previous
  // overlays should have already been handled.
  auto& candidate = surfaces->back();
  if (!gl::SurfaceControl::SupportsColorSpace(candidate.color_space)) {
    DCHECK(!candidate.use_output_surface_for_resource)
        << "The main overlay must only use color space supported by the "
           "device";
    candidate.overlay_handled = false;
    return;
  }

  const gfx::RectF orig_display_rect =
      gfx::RectF(gfx::ToEnclosingRect(candidate.display_rect));

  candidate.display_rect = orig_display_rect;
  if (candidate.is_clipped)
    candidate.display_rect.Intersect(gfx::RectF(candidate.clip_rect));

  // The framework doesn't support display rects positioned at a negative
  // offset.
  candidate.display_rect = ClipFromOrigin(candidate.display_rect);
  if (candidate.display_rect.IsEmpty()) {
    candidate.overlay_handled = false;
    return;
  }

  candidate.uv_rect = cc::MathUtil::ScaleRectProportional(
      candidate.uv_rect, orig_display_rect, candidate.display_rect);
  candidate.overlay_handled = true;

#if DCHECK_IS_ON()
  for (auto& candidate : *surfaces)
    DCHECK(candidate.overlay_handled);
#endif
}

}  // namespace viz
