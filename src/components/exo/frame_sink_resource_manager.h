// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_FRAME_SINK_RESOURCE_MANAGER_H_
#define COMPONENTS_EXO_FRAME_SINK_RESOURCE_MANAGER_H_

#include "base/containers/flat_map.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/returned_resource.h"

namespace exo {

// This class manages the resource IDs and active resource callbacks suitable
// for implementing a frame sink.
class FrameSinkResourceManager {
 public:
  FrameSinkResourceManager();
  ~FrameSinkResourceManager();

  bool HasReleaseCallbackForResource(viz::ResourceId id);
  void SetResourceReleaseCallback(viz::ResourceId id,
                                  viz::ReleaseCallback callback);
  int AllocateResourceId();

  bool HasNoCallbacks() const;
  void ReclaimResource(const viz::ReturnedResource& resource);
  void ClearAllCallbacks();

 private:
  // A collection of callbacks used to release resources.
  using ResourceReleaseCallbackMap =
      base::flat_map<viz::ResourceId, viz::ReleaseCallback>;
  ResourceReleaseCallbackMap release_callbacks_;

  // The next resource id the buffer is attached to.
  int next_resource_id_ = 1;

  DISALLOW_COPY_AND_ASSIGN(FrameSinkResourceManager);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_FRAME_SINK_RESOURCE_MANAGER_H_
