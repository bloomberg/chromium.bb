// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_OFFSCREEN_CANVAS_RESOURCE_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_OFFSCREEN_CANVAS_RESOURCE_PROVIDER_H_

#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace viz {
struct ReturnedResource;
class SingleReleaseCallback;
struct TransferableResource;
}

namespace blink {

class CanvasResource;

class PLATFORM_EXPORT OffscreenCanvasResourceProvider {
 public:
  OffscreenCanvasResourceProvider();
  ~OffscreenCanvasResourceProvider();

  void SetTransferableResource(viz::TransferableResource* out_resource,
                               scoped_refptr<CanvasResource>);

  void ReclaimResource(unsigned resource_id);
  void ReclaimResources(const WTF::Vector<viz::ReturnedResource>& resources);
  void IncNextResourceId() { next_resource_id_++; }
  unsigned GetNextResourceId() { return next_resource_id_; }

 private:
  struct FrameResource {
    FrameResource() = default;
    ~FrameResource();

    // TODO(junov):  What does this do?
    bool spare_lock = true;

    std::unique_ptr<viz::SingleReleaseCallback> release_callback;
    gpu::SyncToken sync_token;
    bool is_lost = false;
  };

  using ResourceMap = HashMap<unsigned, std::unique_ptr<FrameResource>>;

  void SetNeedsBeginFrameInternal();
  std::unique_ptr<FrameResource> CreateOrRecycleFrameResource();
  void ReclaimResourceInternal(const ResourceMap::iterator&);

  unsigned next_resource_id_ = 0;
  std::unique_ptr<FrameResource> recyclable_resource_;
  ResourceMap resources_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_OFFSCREEN_CANVAS_RESOURCE_PROVIDER_H_
