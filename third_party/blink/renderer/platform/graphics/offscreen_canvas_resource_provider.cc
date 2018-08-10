// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/offscreen_canvas_resource_provider.h"

#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_dispatcher.h"

namespace blink {

OffscreenCanvasResourceProvider::OffscreenCanvasResourceProvider() = default;

OffscreenCanvasResourceProvider::~OffscreenCanvasResourceProvider() = default;

std::unique_ptr<OffscreenCanvasResourceProvider::FrameResource>
OffscreenCanvasResourceProvider::CreateOrRecycleFrameResource() {
  if (!recyclable_resource_)
    return std::make_unique<FrameResource>();

  recyclable_resource_->spare_lock = true;
  return std::move(recyclable_resource_);
}

void OffscreenCanvasResourceProvider::SetTransferableResource(
    viz::TransferableResource* out_resource,
    scoped_refptr<CanvasResource> canvas_resource) {
  DCHECK(canvas_resource->IsValid());

  std::unique_ptr<FrameResource> frame_resource =
      CreateOrRecycleFrameResource();

  // TODO(junov): Using verified sync tokens for each offscreencanvas is
  // suboptimal in the case where there are multiple offscreen canvases
  // commiting frames.  Would be more efficient to batch the verifications.
  canvas_resource->PrepareTransferableResource(
      out_resource, &frame_resource->release_callback, kVerifiedSyncToken);
  out_resource->id = next_resource_id_;

  resources_.insert(next_resource_id_, std::move(frame_resource));
}

void OffscreenCanvasResourceProvider::ReclaimResources(
    const WTF::Vector<viz::ReturnedResource>& resources) {
  for (const auto& resource : resources) {
    auto it = resources_.find(resource.id);

    DCHECK(it != resources_.end());
    if (it == resources_.end())
      continue;

    it->value->sync_token = resource.sync_token;
    it->value->is_lost = resource.lost;
    ReclaimResourceInternal(it);
  }
}

void OffscreenCanvasResourceProvider::ReclaimResource(unsigned resource_id) {
  auto it = resources_.find(resource_id);
  if (it != resources_.end())
    ReclaimResourceInternal(it);
}

void OffscreenCanvasResourceProvider::ReclaimResourceInternal(
    const ResourceMap::iterator& it) {
  if (it->value->spare_lock) {
    it->value->spare_lock = false;
    return;
  }

  if (it->value->release_callback)
    it->value->release_callback->Run(it->value->sync_token, it->value->is_lost);

  // Recycle resource.
  recyclable_resource_ = std::move(it->value);
  recyclable_resource_->release_callback = nullptr;
  recyclable_resource_->sync_token.Clear();
  recyclable_resource_->is_lost = false;
  resources_.erase(it);
}

OffscreenCanvasResourceProvider::FrameResource::~FrameResource() {
  if (release_callback)
    release_callback->Run(sync_token, is_lost);
}

}  // namespace blink
