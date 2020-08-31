// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_STUB_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_STUB_H_

#include "components/viz/service/display/overlay_processor_interface.h"

namespace viz {
// This is a stub class that implements OverlayProcessorInterface that is used
// for platforms that don't support overlays.
class VIZ_SERVICE_EXPORT OverlayProcessorStub
    : public OverlayProcessorInterface {
 public:
  OverlayProcessorStub() : OverlayProcessorInterface() {}
  ~OverlayProcessorStub() override {}

  // Overrides OverlayProcessorInterface's pure virtual functions.
  bool IsOverlaySupported() const final;
  gfx::Rect GetAndResetOverlayDamage() final;
  bool NeedsSurfaceOccludingDamageRect() const final;
  void ProcessForOverlays(
      DisplayResourceProvider* resource_provider,
      RenderPassList* render_passes,
      const SkMatrix44& output_color_matrix,
      const FilterOperationsMap& render_pass_filters,
      const FilterOperationsMap& render_pass_backdrop_filters,
      OutputSurfaceOverlayPlane* output_surface_plane,
      CandidateList* overlay_candidates,
      gfx::Rect* damage_rect,
      std::vector<gfx::Rect>* content_bounds) final {}
  void AdjustOutputSurfaceOverlay(
      base::Optional<OutputSurfaceOverlayPlane>* output_surface_plane) final {}
  void SetDisplayTransformHint(gfx::OverlayTransform transform) final {}
  void SetViewportSize(const gfx::Size& size) final {}

 private:
  DISALLOW_COPY_AND_ASSIGN(OverlayProcessorStub);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_STUB_H_
