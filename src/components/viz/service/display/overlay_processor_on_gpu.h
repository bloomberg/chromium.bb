// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_ON_GPU_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_ON_GPU_H_

#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/viz_service_export.h"

#if defined(OS_WIN)
#include "components/viz/service/display/dc_layer_overlay.h"
#endif

#if defined(OS_MACOSX)
#include "components/viz/service/display/ca_layer_overlay.h"
#endif

namespace gpu {
class MemoryTracker;
class SharedImageManager;
class SharedImageRepresentationFactory;
}  // namespace gpu

namespace viz {
// This class defines the gpu thread side functionalities of overlay processing.
// This class would receive a list of overlay candidates and schedule to present
// the overlay candidates every frame. This class is created, accessed, and
// destroyed on the gpu thread.
class VIZ_SERVICE_EXPORT OverlayProcessorOnGpu {
 public:
#if defined(OS_MACOSX)
  using CandidateList = CALayerOverlayList;
#elif defined(OS_WIN)
  using CandidateList = DCLayerOverlayList;
#else
  // Default.
  using CandidateList = OverlayCandidateList;
#endif

  OverlayProcessorOnGpu(gpu::SharedImageManager* shared_image_manager,
                        gpu::MemoryTracker* memory_tracker);
  ~OverlayProcessorOnGpu();

  // This function takes the overlay candidates, and schedule them for
  // presentation later.
  void ScheduleOverlays(CandidateList&& overlay_candidates);

#if defined(OS_ANDROID)
  void NotifyOverlayPromotions(
      base::flat_set<gpu::Mailbox> promotion_denied,
      base::flat_map<gpu::Mailbox, gfx::Rect> possible_promotions);
#endif

 private:
  // TODO(weiliangc): Figure out how to share MemoryTracker with OutputSurface.
  // For now this class is only used for Android classic code path, which only
  // reads the shared images created elsewhere.
  std::unique_ptr<gpu::SharedImageRepresentationFactory>
      shared_image_representation_factory_;
  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(OverlayProcessorOnGpu);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_ON_GPU_H_
