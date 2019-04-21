// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/gl_output_surface_win.h"

#include "components/viz/service/display_embedder/compositor_overlay_candidate_validator_win.h"

namespace viz {

GLOutputSurfaceWin::GLOutputSurfaceWin(
    scoped_refptr<VizProcessContextProvider> context_provider,
    UpdateVSyncParametersCallback update_vsync_callback,
    bool use_overlays)
    : GLOutputSurface(context_provider, std::move(update_vsync_callback)) {
  if (use_overlays) {
    overlay_validator_ =
        std::make_unique<CompositorOverlayCandidateValidatorWin>();
  }
}

GLOutputSurfaceWin::~GLOutputSurfaceWin() = default;

OverlayCandidateValidator* GLOutputSurfaceWin::GetOverlayCandidateValidator()
    const {
  return overlay_validator_.get();
}

}  // namespace viz
