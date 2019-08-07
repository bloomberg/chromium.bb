// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_SURFACE_RESOURCE_HOLDER_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_SURFACE_RESOURCE_HOLDER_H_

#include <unordered_map>

#include "base/macros.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {
class SurfaceResourceHolderClient;

// A SurfaceResourceHolder manages the lifetime of resources submitted by a
// particular SurfaceFactory. Each resource is held by the service until
// it is no longer referenced by any pending frames or by any
// resource providers.
class VIZ_SERVICE_EXPORT SurfaceResourceHolder {
 public:
  explicit SurfaceResourceHolder(SurfaceResourceHolderClient* client);
  ~SurfaceResourceHolder();

  void Reset();
  void ReceiveFromChild(const std::vector<TransferableResource>& resources);
  void RefResources(const std::vector<TransferableResource>& resources);
  void UnrefResources(const std::vector<ReturnedResource>& resources);

 private:
  SurfaceResourceHolderClient* client_;

  struct ResourceRefs {
    ResourceRefs();

    int refs_received_from_child;
    int refs_holding_resource_alive;
    gpu::SyncToken sync_token;
  };
  // Keeps track of the number of users currently in flight for each resource
  // ID we've received from the client. When this counter hits zero for a
  // particular resource, that ID is available to return to the client with
  // the most recently given non-empty sync token.
  using ResourceIdInfoMap = std::unordered_map<ResourceId, ResourceRefs>;
  ResourceIdInfoMap resource_id_info_map_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceResourceHolder);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_SURFACE_RESOURCE_HOLDER_H_
