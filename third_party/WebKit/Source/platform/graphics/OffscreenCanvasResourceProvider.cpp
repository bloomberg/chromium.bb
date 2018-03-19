// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/OffscreenCanvasResourceProvider.h"

#include "base/numerics/checked_math.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "platform/graphics/gpu/SharedGpuContext.h"
#include "platform/wtf/typed_arrays/ArrayBuffer.h"
#include "platform/wtf/typed_arrays/Uint8Array.h"
#include "public/platform/Platform.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSwizzle.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace blink {

OffscreenCanvasResourceProvider::OffscreenCanvasResourceProvider(int width,
                                                                 int height)
    : width_(width), height_(height), next_resource_id_(0u) {}

OffscreenCanvasResourceProvider::~OffscreenCanvasResourceProvider() = default;

std::unique_ptr<OffscreenCanvasResourceProvider::FrameResource>
OffscreenCanvasResourceProvider::CreateOrRecycleFrameResource() {
  if (recyclable_resource_) {
    recyclable_resource_->spare_lock_ = true;
    return std::move(recyclable_resource_);
  }
  return std::make_unique<FrameResource>();
}

void OffscreenCanvasResourceProvider::TransferResource(
    viz::TransferableResource* resource) {
  resource->id = next_resource_id_;
  resource->format = viz::ResourceFormat::RGBA_8888;
  resource->size = gfx::Size(width_, height_);
  // This indicates the filtering on the resource inherently, not the desired
  // filtering effect on the quad.
  resource->filter = GL_NEAREST;
  // TODO(crbug.com/646022): making this overlay-able.
  resource->is_overlay_candidate = false;
}

// TODO(xlai): Handle error cases when, by any reason,
// OffscreenCanvasResourceProvider fails to get image data.
void OffscreenCanvasResourceProvider::SetTransferableResourceToSharedBitmap(
    viz::TransferableResource& resource,
    scoped_refptr<StaticBitmapImage> image) {
  std::unique_ptr<FrameResource> frame_resource =
      CreateOrRecycleFrameResource();
  if (!frame_resource->shared_bitmap_) {
    frame_resource->shared_bitmap_ = Platform::Current()->AllocateSharedBitmap(
        IntSize(width_, height_), viz::ResourceFormat::RGBA_8888);
    if (!frame_resource->shared_bitmap_)
      return;
  }
  unsigned char* pixels = frame_resource->shared_bitmap_->pixels();
  DCHECK(pixels);
  // TODO(xlai): Optimize to avoid copying pixels. See crbug.com/651456.
  // However, in the case when |image| is texture backed, this function call
  // does a GPU readback which is required.
  sk_sp<SkImage> sk_image = image->PaintImageForCurrentFrame().GetSkImage();
  if (sk_image->bounds().isEmpty())
    return;
  SkImageInfo image_info = SkImageInfo::Make(
      width_, height_, kN32_SkColorType,
      image->IsPremultiplied() ? kPremul_SkAlphaType : kUnpremul_SkAlphaType,
      sk_image->refColorSpace());
  if (image_info.isEmpty())
    return;
  bool read_pixels_successful =
      sk_image->readPixels(image_info, pixels, image_info.minRowBytes(), 0, 0);
  DCHECK(read_pixels_successful);
  if (!read_pixels_successful)
    return;
  resource.mailbox_holder.mailbox = frame_resource->shared_bitmap_->id();
  resource.mailbox_holder.texture_target = 0;
  resource.is_software = true;

  resources_.insert(next_resource_id_, std::move(frame_resource));
}

void OffscreenCanvasResourceProvider::
    SetTransferableResourceToStaticBitmapImage(
        viz::TransferableResource& resource,
        scoped_refptr<StaticBitmapImage> image) {
  DCHECK(image->IsTextureBacked());
  DCHECK(image->IsValid());
  image->EnsureMailbox(kVerifiedSyncToken, GL_LINEAR);
  resource.mailbox_holder = gpu::MailboxHolder(
      image->GetMailbox(), image->GetSyncToken(), GL_TEXTURE_2D);
  resource.read_lock_fences_enabled = false;
  resource.is_software = false;

  std::unique_ptr<FrameResource> frame_resource =
      CreateOrRecycleFrameResource();

  frame_resource->image_ = std::move(image);
  resources_.insert(next_resource_id_, std::move(frame_resource));
}

OffscreenCanvasResourceProvider::FrameResource::~FrameResource() = default;

void OffscreenCanvasResourceProvider::ReclaimResources(
    const WTF::Vector<viz::ReturnedResource>& resources) {
  for (const auto& resource : resources) {
    auto it = resources_.find(resource.id);

    DCHECK(it != resources_.end());
    if (it == resources_.end())
      continue;

    if (it->value->image_ && it->value->image_->ContextProviderWrapper() &&
        resource.sync_token.HasData()) {
      it->value->image_->ContextProviderWrapper()
          ->ContextProvider()
          ->ContextGL()
          ->WaitSyncTokenCHROMIUM(resource.sync_token.GetConstData());
    }
    ReclaimResourceInternal(it);
  }
}

void OffscreenCanvasResourceProvider::ReclaimResource(unsigned resource_id) {
  auto it = resources_.find(resource_id);
  if (it != resources_.end()) {
    ReclaimResourceInternal(it);
  }
}

void OffscreenCanvasResourceProvider::ReclaimResourceInternal(
    const ResourceMap::iterator& it) {
  if (it->value->spare_lock_) {
    it->value->spare_lock_ = false;
  } else {
    // Really reclaim the resources
    recyclable_resource_ = std::move(it->value);
    // release SkImage immediately since it is not recyclable
    recyclable_resource_->image_ = nullptr;
    resources_.erase(it);
  }
}

}  // namespace blink
