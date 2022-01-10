// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_RESOLVED_FRAME_DATA_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_RESOLVED_FRAME_DATA_H_

#include <unordered_map>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class Surface;

// Returns |damage_rect| field from the DrawQuad if it exists otherwise returns
// an empty optional.
const absl::optional<gfx::Rect>& GetOptionalDamageRectFromQuad(
    const DrawQuad* quad);

// Data associated with a DrawQuad in a resolved frame.
struct VIZ_SERVICE_EXPORT ResolvedQuadData {
  explicit ResolvedQuadData(const DrawQuad& quad);

  // Remapped display ResourceIds.
  DrawQuad::Resources remapped_resources;
};

// Render pass data that is fixed for the lifetime of ResolvedPassData.
struct VIZ_SERVICE_EXPORT FixedPassData {
  FixedPassData();
  FixedPassData(FixedPassData&& other);
  FixedPassData& operator=(FixedPassData&& other);
  ~FixedPassData();

  raw_ptr<CompositorRenderPass> render_pass = nullptr;
  // DrawQuads in |render_pass| that can contribute additional damage (eg.
  // surface and render passes) that need to be visited during the prewalk phase
  // of aggregation. Stored in front-to-back order like in |render_pass|.
  std::vector<const DrawQuad*> prewalk_quads;

  AggregatedRenderPassId remapped_id;
  bool is_root = false;
  std::vector<ResolvedQuadData> draw_quads;
};

class ResolvedPassData;

// Render pass data that must be recomputed each aggregation. Unlike
// FixedPassData this changes each aggregation depending on what other
// Surfaces/CompositorFrames are part of the draw tree.
struct VIZ_SERVICE_EXPORT AggregationPassData {
  AggregationPassData();
  AggregationPassData(AggregationPassData&& other);
  AggregationPassData& operator=(AggregationPassData&& other);
  ~AggregationPassData();

  // Resets to default constructed state.
  void Reset();

  // Embedded render passes that contribute pixels to this render pass.
  base::flat_set<ResolvedPassData*> embedded_passes;

  // True if the render pass is drawn to fulfil part of a copy request. This
  // property is transitive from parent pass to embedded passes.
  bool in_copy_request_pass = false;

  // True if the render pass is be impacted by a pixel moving foreground filter.
  // This property is transitive from parent pass to embedded passes.
  bool in_pixel_moving_filter_pass = false;

  // True if the render pass will be stored as part of a cached render pass.
  // This property is transitive from parent pass to embedded passes.
  bool in_cached_render_pass = false;

  // True if there is accumulated damage from contributing render pass or
  // surface quads.
  bool has_damage_from_contributing_content = false;
};

// Data associated with a CompositorRenderPass in a resolved frame. Has fixed
// portion that does not change and an aggregation portion that does change.
class VIZ_SERVICE_EXPORT ResolvedPassData {
 public:
  explicit ResolvedPassData(FixedPassData fixed_data);
  ~ResolvedPassData();
  ResolvedPassData(ResolvedPassData&& other);
  ResolvedPassData& operator=(ResolvedPassData&& other);

  const CompositorRenderPass& render_pass() const {
    return *fixed_.render_pass;
  }
  AggregatedRenderPassId remapped_id() const { return fixed_.remapped_id; }
  bool is_root() const { return fixed_.is_root; }
  const std::vector<ResolvedQuadData>& draw_quads() const {
    return fixed_.draw_quads;
  }
  const std::vector<const DrawQuad*>& prewalk_quads() const {
    return fixed_.prewalk_quads;
  }

  AggregationPassData& aggregation() { return aggregation_; }
  const AggregationPassData& aggregation() const { return aggregation_; }

 private:
  // Data that is constant for the life of the resolved pass.
  FixedPassData fixed_;

  // Data that will change each aggregation.
  AggregationPassData aggregation_;
};

// Holds computed information for a particular Surface+CompositorFrame. The
// CompositorFrame computed information will be updated whenever the active
// frame for the surface has changed.
class VIZ_SERVICE_EXPORT ResolvedFrameData {
 public:
  ResolvedFrameData(const SurfaceId& surface_id, Surface* surface);
  ~ResolvedFrameData();
  ResolvedFrameData(ResolvedFrameData&& other) = delete;
  ResolvedFrameData& operator=(ResolvedFrameData&& other) = delete;

  const SurfaceId& surface_id() const { return surface_id_; }
  Surface* surface() const { return surface_; }
  bool is_valid() const { return valid_; }
  uint64_t frame_index() const { return frame_index_; }

  // Updates resolved frame data for a new active frame. This will recompute
  // ResolvedPassData. |child_to_parent_map| is the ResourceId mapping provided
  // from DisplayResourceProvider which includes all of ResourceIds referenced
  // by quads in the active frame. Returns all ResourceIds that are used in the
  // active frame.
  //
  // This performs the following validation on the active CompositorFrame.
  // 1. Checks each ResourceId was registered with DisplayResourceProvider and
  //    is in |child_to_parent_map|.
  // 2. Checks that CompositorRenderPasses have unique ids.
  // 3. Checks that CompositorRenderPassDrawQuads only embed render passes that
  //    are drawn before. This has the side effect of disallowing any cycles.
  //
  // If validation fails then an empty set of resources will be returned, all
  // ResolvedPassData will be cleared and is_valid() will return false.
  ResourceIdSet UpdateForActiveFrame(
      const std::unordered_map<ResourceId, ResourceId, ResourceIdHasher>&
          child_to_parent_map,
      AggregatedRenderPassId::Generator& render_pass_id_generator);

  // Sets frame index and marks as invalid. This also clears any existing
  // resolved pass data.
  void SetInvalid();

  // Marks this as used and returns true if this was the first time MarkAsUsed()
  // was called since last reset.
  bool MarkAsUsed();

  // Returns true if MarkAsUsed() was called since last reset and then resets
  // used to false.
  bool CheckIfUsedAndReset();

  // All functions after this point are accessors for the resolved frame and
  // should only be called if is_valid() returns true.

  // RenderPassData accessors.
  ResolvedPassData& GetRenderPassDataById(
      CompositorRenderPassId render_pass_id);
  const ResolvedPassData& GetRenderPassDataById(
      CompositorRenderPassId render_pass_id) const;

  ResolvedPassData& GetRootRenderPassData();
  const ResolvedPassData& GetRootRenderPassData() const;

  std::vector<ResolvedPassData>& GetResolvedPasses() {
    return resolved_passes_;
  }
  const std::vector<ResolvedPassData>& GetResolvedPasses() const {
    return resolved_passes_;
  }

  // Returns active frame damage rect. If |include_per_quad_damage| then the
  // damage_rect will include unioned per quad damage, otherwise it will be
  // limited to the root render passes damage_rect.
  const gfx::Rect& GetDamageRect(bool include_per_quad_damage) const;

  // Returns the root render pass output_rect.
  const gfx::Rect& GetOutputRect() const;

 private:
  const SurfaceId surface_id_;
  const raw_ptr<Surface> surface_;

  // Data associated with CompositorFrame with |frame_index_|.
  bool valid_ = false;
  uint64_t frame_index_ = 0;
  std::vector<ResolvedPassData> resolved_passes_;
  base::flat_map<CompositorRenderPassId, ResolvedPassData*> render_pass_id_map_;
  base::flat_map<CompositorRenderPassId, AggregatedRenderPassId>
      aggregated_id_map_;
  gfx::Rect root_damage_rect_;

  // Track if the this resolved frame was used this frame.
  bool used_ = false;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_RESOLVED_FRAME_DATA_H_
