// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_SURFACES_LOCAL_SURFACE_ID_ALLOCATION_H_
#define COMPONENTS_VIZ_COMMON_SURFACES_LOCAL_SURFACE_ID_ALLOCATION_H_

#include <ostream>
#include <string>

#include "base/time/time.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/viz_common_export.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace viz {

namespace mojom {
class LocalSurfaceIdAllocationDataView;
}

class ChildLocalSurfaceIdAllocator;
class ParentLocalSurfaceIdAllocator;

// This tracks information related to the allocation of a LocalSurfaceId.
// It holds both the LocalSurfaceId, along with the allocation time.
class VIZ_COMMON_EXPORT LocalSurfaceIdAllocation {
 public:
  constexpr LocalSurfaceIdAllocation() = default;

  LocalSurfaceIdAllocation(const LocalSurfaceId& local_surface_id,
                           base::TimeTicks allocation_time);

  // Returns true if both |local_surface_id_| is valid, and a non-null
  // |allocation_time_| was provided.
  bool IsValid() const;
  std::string ToString() const;

  const LocalSurfaceId& local_surface_id() const { return local_surface_id_; }
  base::TimeTicks allocation_time() const { return allocation_time_; }

  bool operator==(const LocalSurfaceIdAllocation& other) const {
    return local_surface_id_ == other.local_surface_id_ &&
           allocation_time_ == other.allocation_time_;
  }

  bool operator!=(const LocalSurfaceIdAllocation& other) const {
    return !(*this == other);
  }

 private:
  friend struct mojo::StructTraits<mojom::LocalSurfaceIdAllocationDataView,
                                   LocalSurfaceIdAllocation>;
  friend class ChildLocalSurfaceIdAllocator;
  friend class ParentLocalSurfaceIdAllocator;

  LocalSurfaceId local_surface_id_;
  base::TimeTicks allocation_time_;
};

VIZ_COMMON_EXPORT std::ostream& operator<<(
    std::ostream& stream,
    const LocalSurfaceIdAllocation& allocation);

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_SURFACES_LOCAL_SURFACE_ID_ALLOCATION_H_
