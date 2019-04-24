// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_ALLOCATION_GROUP_H_
#define COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_ALLOCATION_GROUP_H_

#include <vector>

#include "base/unguessable_token.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/surface_range.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class Surface;

// This class keeps track of the LocalSurfaceIds that were generated using the
// same ParentLocalSurfaceIdAllocator (i.e. have the same embed token).
class VIZ_SERVICE_EXPORT SurfaceAllocationGroup {
 public:
  SurfaceAllocationGroup(const FrameSinkId& submitter,
                         const base::UnguessableToken& embed_token);
  ~SurfaceAllocationGroup();

  // Returns the ID of the FrameSink that is submitting to surfaces in this
  // allocation group.
  const FrameSinkId& submitter_frame_sink_id() const { return submitter_; }

  // Returns whether this SurfaceAllocationGroup can be destroyed by the garbage
  // collector; that is, there are no surfaces left in this allocation group.
  // TODO(samans): Also take into account the observers once
  // SurfaceAllocationGroups can be observed.
  bool IsReadyToDestroy() const;

  // Called by |surface| at construction time to register itself in this
  // allocation group.
  void RegisterSurface(Surface* surface);

  // Called by |surface| at destruction time to unregister itself from this
  // allocation group.
  void UnregisterSurface(Surface* surface);

  // Returns the latest active surface in the given range that is a part of this
  // allocation group. The embed token of at least one end of the range must
  // match the embed token of this group.
  Surface* FindLatestActiveSurfaceInRange(const SurfaceRange& range) const;

  // Returns the last surface created in this allocation group.
  Surface* last_created_surface() const {
    return surfaces_.empty() ? nullptr : surfaces_.back();
  }

 private:
  // Helper method for FindLatestActiveSurfaceInRange. Returns the latest active
  // surface whose SurfaceId is older than or equal to |surface_id|.
  Surface* FindOlderOrEqual(const SurfaceId& surface_id) const;

  // The ID of the FrameSink that is submitting to the surfaces in this
  // allocation group.
  const FrameSinkId submitter_;

  // The embed token that the ParentLocalSurfaceIdAllocator used to allocate
  // LocalSurfaceIds for the surfaces in this allocation group. All the surfaces
  // in this allocation group use this embed token.
  const base::UnguessableToken embed_token_;

  // The list of surfaces in this allocation group in the order of creation. The
  // parent and child sequence numbers of these surfaces is monotonically
  // increasing.
  std::vector<Surface*> surfaces_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceAllocationGroup);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_ALLOCATION_GROUP_H_
