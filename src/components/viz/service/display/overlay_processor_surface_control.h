// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_SURFACE_CONTROL_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_SURFACE_CONTROL_H_

#include "components/viz/service/display/overlay_processor_using_strategy.h"

namespace viz {

// This is an overlay processor implementation for Android SurfaceControl.
class VIZ_SERVICE_EXPORT OverlayProcessorSurfaceControl
    : public OverlayProcessorUsingStrategy {
 public:
  explicit OverlayProcessorSurfaceControl(bool enable_overlay);
  ~OverlayProcessorSurfaceControl() override;

  bool IsOverlaySupported() const override;

  bool NeedsSurfaceOccludingDamageRect() const override;

  // Override OverlayProcessorUsingStrategy.
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override;
  void SetViewportSize(const gfx::Size& size) override;
  void AdjustOutputSurfaceOverlay(
      base::Optional<OutputSurfaceOverlayPlane>* output_surface_plane) override;
  void CheckOverlaySupport(
      const OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
      OverlayCandidateList* candidates) override;
  gfx::Rect GetOverlayDamageRectForOutputSurface(
      const OverlayCandidate& overlay) const override;

 private:
  const bool overlay_enabled_;
  gfx::OverlayTransform display_transform_ = gfx::OVERLAY_TRANSFORM_NONE;
  gfx::Size viewport_size_;
};
}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_SURFACE_CONTROL_H_
