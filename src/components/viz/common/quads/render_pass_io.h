// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_RENDER_PASS_IO_H_
#define COMPONENTS_VIZ_COMMON_QUADS_RENDER_PASS_IO_H_

#include <memory>
#include <vector>

#include "base/values.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/viz_common_export.h"

namespace viz {
VIZ_COMMON_EXPORT base::Value CompositorRenderPassToDict(
    const CompositorRenderPass& render_pass);
VIZ_COMMON_EXPORT std::unique_ptr<CompositorRenderPass>
CompositorRenderPassFromDict(const base::Value& dict);

VIZ_COMMON_EXPORT base::Value CompositorRenderPassListToDict(
    const CompositorRenderPassList& render_pass_list);
VIZ_COMMON_EXPORT bool CompositorRenderPassListFromDict(
    const base::Value& dict,
    CompositorRenderPassList* render_pass_list);

// Represents the important information used for (de)serialization of
// `CompositorFrame`s on a given surface.
struct VIZ_COMMON_EXPORT FrameData {
  FrameData();
  FrameData(const SurfaceId& surface_id,
            const uint64_t frame_index,
            const CompositorFrame& compositor_frame);
  FrameData(FrameData&& other);
  FrameData& operator=(FrameData&& other);

  SurfaceId surface_id;
  uint64_t frame_index;
  CompositorFrame compositor_frame;
};

// These functions (de)serialize data about CompositorFrames for multiple
// surfaces, represented as arrays of `FrameData`s.
VIZ_COMMON_EXPORT base::Value FrameDataToList(
    const std::vector<FrameData>& frame_data_list);
VIZ_COMMON_EXPORT bool FrameDataFromList(
    const base::Value& list,
    std::vector<FrameData>* frame_data_list);
}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_RENDER_PASS_IO_H_
