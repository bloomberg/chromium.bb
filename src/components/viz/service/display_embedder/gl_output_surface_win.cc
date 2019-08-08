// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/gl_output_surface_win.h"

#include "components/viz/service/display_embedder/overlay_candidate_validator_win.h"

namespace viz {

GLOutputSurfaceWin::GLOutputSurfaceWin(
    scoped_refptr<VizProcessContextProvider> context_provider,
    bool use_overlays)
    : GLOutputSurface(context_provider) {
  if (use_overlays) {
    overlay_validator_ = std::make_unique<OverlayCandidateValidatorWin>();
  }
}

GLOutputSurfaceWin::~GLOutputSurfaceWin() = default;

std::unique_ptr<OverlayCandidateValidator>
GLOutputSurfaceWin::TakeOverlayCandidateValidator() {
  return std::move(overlay_validator_);
}

}  // namespace viz
