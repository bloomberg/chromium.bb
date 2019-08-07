// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/gl_output_surface_ozone.h"

#include <utility>

#include "components/viz/service/display_embedder/overlay_candidate_validator_ozone.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"
#include "ui/ozone/public/overlay_manager_ozone.h"
#include "ui/ozone/public/ozone_platform.h"

namespace viz {

GLOutputSurfaceOzone::GLOutputSurfaceOzone(
    scoped_refptr<VizProcessContextProvider> context_provider,
    gpu::SurfaceHandle surface_handle,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    std::vector<OverlayStrategy> strategies)
    : GLOutputSurfaceBufferQueue(context_provider,
                                 surface_handle,
                                 gpu_memory_buffer_manager,
                                 display::DisplaySnapshot::PrimaryFormat()) {
  if (!strategies.empty()) {
    auto* overlay_manager =
        ui::OzonePlatform::GetInstance()->GetOverlayManager();
    std::unique_ptr<ui::OverlayCandidatesOzone> overlay_candidates =
        overlay_manager->CreateOverlayCandidates(surface_handle);
    overlay_candidate_validator_ =
        std::make_unique<OverlayCandidateValidatorOzone>(
            std::move(overlay_candidates), std::move(strategies));
  }
}

GLOutputSurfaceOzone::~GLOutputSurfaceOzone() = default;

std::unique_ptr<OverlayCandidateValidator>
GLOutputSurfaceOzone::TakeOverlayCandidateValidator() {
  return std::move(overlay_candidate_validator_);
}

}  // namespace viz
