// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/gl_output_surface_android.h"

#include "components/viz/service/display_embedder/compositor_overlay_candidate_validator_android.h"

namespace viz {

GLOutputSurfaceAndroid::GLOutputSurfaceAndroid(
    scoped_refptr<VizProcessContextProvider> context_provider,
    SyntheticBeginFrameSource* synthetic_begin_frame_source,
    bool allow_overlays)
    : GLOutputSurface(context_provider, synthetic_begin_frame_source) {
  if (allow_overlays) {
    overlay_candidate_validator_ =
        std::make_unique<CompositorOverlayCandidateValidatorAndroid>();
  }
}

GLOutputSurfaceAndroid::~GLOutputSurfaceAndroid() = default;

void GLOutputSurfaceAndroid::HandlePartialSwap(
    const gfx::Rect& sub_buffer_rect,
    uint32_t flags,
    gpu::ContextSupport::SwapCompletedCallback swap_callback,
    gpu::ContextSupport::PresentationCallback presentation_callback) {
  DCHECK(sub_buffer_rect.IsEmpty());
  context_provider_->ContextSupport()->CommitOverlayPlanes(
      flags, std::move(swap_callback), std::move(presentation_callback));
}

OverlayCandidateValidator*
GLOutputSurfaceAndroid::GetOverlayCandidateValidator() const {
  return overlay_candidate_validator_.get();
}

}  // namespace viz
