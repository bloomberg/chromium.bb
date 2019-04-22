// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_CLIENT_LOCAL_SURFACE_ID_PROVIDER_H_
#define COMPONENTS_VIZ_CLIENT_LOCAL_SURFACE_ID_PROVIDER_H_

#include "components/viz/client/viz_client_export.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "ui/gfx/geometry/size.h"

namespace viz {
class CompositorFrame;

class VIZ_CLIENT_EXPORT LocalSurfaceIdProvider {
 public:
  LocalSurfaceIdProvider();
  virtual ~LocalSurfaceIdProvider();

  virtual const LocalSurfaceIdAllocation&
  GetLocalSurfaceIdAllocationForFrame(const CompositorFrame& frame) = 0;

  void ForceAllocateNewId();

 protected:
  ParentLocalSurfaceIdAllocator parent_local_surface_id_allocator_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LocalSurfaceIdProvider);
};

}  //  namespace viz

#endif  // COMPONENTS_VIZ_CLIENT_LOCAL_SURFACE_ID_PROVIDER_H_
