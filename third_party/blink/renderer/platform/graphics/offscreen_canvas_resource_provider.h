// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_OFFSCREEN_CANVAS_RESOURCE_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_OFFSCREEN_CANVAS_RESOURCE_PROVIDER_H_

#include "components/viz/common/quads/shared_bitmap.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"

namespace blink {

class PLATFORM_EXPORT OffscreenCanvasResourceProvider {
 public:
  OffscreenCanvasResourceProvider(int width, int height);

  ~OffscreenCanvasResourceProvider();

  void TransferResource(viz::TransferableResource*);
  void SetTransferableResourceToSharedBitmap(viz::TransferableResource&,
                                             scoped_refptr<StaticBitmapImage>);
  void SetTransferableResourceToStaticBitmapImage(
      viz::TransferableResource&,
      scoped_refptr<StaticBitmapImage>);

  void ReclaimResource(unsigned resource_id);
  void ReclaimResources(const WTF::Vector<viz::ReturnedResource>& resources);
  void IncNextResourceId() { next_resource_id_++; }
  unsigned GetNextResourceId() { return next_resource_id_; }

  void Reshape(int width, int height) {
    width_ = width;
    height_ = height;
  }

 private:
  int width_;
  int height_;
  unsigned next_resource_id_;

  struct FrameResource {
    scoped_refptr<StaticBitmapImage> image_;
    std::unique_ptr<viz::SharedBitmap> shared_bitmap_;

    bool spare_lock_ = true;
    gpu::Mailbox mailbox_;

    FrameResource() = default;
    ~FrameResource();
  };

  std::unique_ptr<FrameResource> recyclable_resource_;
  std::unique_ptr<FrameResource> CreateOrRecycleFrameResource();

  void SetNeedsBeginFrameInternal();

  typedef HashMap<unsigned, std::unique_ptr<FrameResource>> ResourceMap;
  void ReclaimResourceInternal(const ResourceMap::iterator&);
  ResourceMap resources_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_OFFSCREEN_CANVAS_RESOURCE_PROVIDER_H_
