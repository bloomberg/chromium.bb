// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/OffscreenCanvasResourceProvider.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "platform/graphics/gpu/SharedGpuContext.h"
#include "platform/wtf/typed_arrays/ArrayBuffer.h"
#include "platform/wtf/typed_arrays/Uint8Array.h"
#include "public/platform/Platform.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace blink {

OffscreenCanvasResourceProvider::OffscreenCanvasResourceProvider(int width,
                                                                 int height)
    : width_(width), height_(height), next_resource_id_(1u) {}

OffscreenCanvasResourceProvider::~OffscreenCanvasResourceProvider() {}

std::unique_ptr<OffscreenCanvasResourceProvider::FrameResource>
OffscreenCanvasResourceProvider::CreateOrRecycleFrameResource() {
  if (recycleable_resource_) {
    recycleable_resource_->spare_lock_ = true;
    return std::move(recycleable_resource_);
  }
  return std::unique_ptr<FrameResource>(new FrameResource());
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

void OffscreenCanvasResourceProvider::SetTransferableResourceToSharedBitmap(
    viz::TransferableResource& resource,
    RefPtr<StaticBitmapImage> image) {
  std::unique_ptr<FrameResource> frame_resource =
      CreateOrRecycleFrameResource();
  if (!frame_resource->shared_bitmap_) {
    frame_resource->shared_bitmap_ =
        Platform::Current()->AllocateSharedBitmap(IntSize(width_, height_));
    if (!frame_resource->shared_bitmap_)
      return;
  }
  unsigned char* pixels = frame_resource->shared_bitmap_->pixels();
  DCHECK(pixels);
  SkImageInfo image_info = SkImageInfo::Make(
      width_, height_, kN32_SkColorType,
      image->IsPremultiplied() ? kPremul_SkAlphaType : kUnpremul_SkAlphaType);
  // TODO(xlai): Optimize to avoid copying pixels. See crbug.com/651456.
  // However, in the case when |image| is texture backed, this function call
  // does a GPU readback which is required.
  image->PaintImageForCurrentFrame().GetSkImage()->readPixels(
      image_info, pixels, image_info.minRowBytes(), 0, 0);
  resource.mailbox_holder.mailbox = frame_resource->shared_bitmap_->id();
  resource.mailbox_holder.texture_target = 0;
  resource.is_software = true;

  resources_.insert(next_resource_id_, std::move(frame_resource));
}

void OffscreenCanvasResourceProvider::SetTransferableResourceToSharedGPUContext(
    viz::TransferableResource& resource,
    RefPtr<StaticBitmapImage> image) {
  DCHECK(!image->IsTextureBacked());

  // TODO(crbug.com/652707): When committing the first frame, there is no
  // instance of SharedGpuContext yet, calling SharedGpuContext::gl() will
  // trigger a creation of an instace, which requires to create a
  // WebGraphicsContext3DProvider. This process is quite expensive, because
  // WebGraphicsContext3DProvider can only be constructed on the main thread,
  // and bind to the worker thread if commit() is called on worker. In the
  // subsequent frame, we should already have a SharedGpuContext, then getting
  // the gl interface should not be expensive.
  WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper =
      SharedGpuContext::ContextProviderWrapper();
  if (!context_provider_wrapper)
    return;

  gpu::gles2::GLES2Interface* gl =
      context_provider_wrapper->ContextProvider()->ContextGL();
  GrContext* gr = context_provider_wrapper->ContextProvider()->GetGrContext();
  if (!gl || !gr)
    return;

  std::unique_ptr<FrameResource> frame_resource =
      CreateOrRecycleFrameResource();

  SkImageInfo info = SkImageInfo::Make(
      width_, height_, kN32_SkColorType,
      image->IsPremultiplied() ? kPremul_SkAlphaType : kUnpremul_SkAlphaType);
  RefPtr<ArrayBuffer> dst_buffer =
      ArrayBuffer::CreateOrNull(width_ * height_, info.bytesPerPixel());
  // If it fails to create a buffer for copying the pixel data, then exit early.
  if (!dst_buffer)
    return;
  unsigned byte_length = dst_buffer->ByteLength();
  RefPtr<Uint8Array> dst_pixels =
      Uint8Array::Create(std::move(dst_buffer), 0, byte_length);
  image->PaintImageForCurrentFrame().GetSkImage()->readPixels(
      info, dst_pixels->Data(), info.minRowBytes(), 0, 0);
  DCHECK(frame_resource->context_provider_wrapper_.get() ==
             context_provider_wrapper.get() ||
         !frame_resource->context_provider_wrapper_);

  if (frame_resource->texture_id_ == 0u ||
      !frame_resource->context_provider_wrapper_) {
    frame_resource->context_provider_wrapper_ = context_provider_wrapper;
    gl->GenTextures(1, &frame_resource->texture_id_);
    gl->BindTexture(GL_TEXTURE_2D, frame_resource->texture_id_);
    GLenum format =
        (kN32_SkColorType == kRGBA_8888_SkColorType) ? GL_RGBA : GL_BGRA_EXT;
    gl->TexImage2D(GL_TEXTURE_2D, 0, format, width_, height_, 0, format,
                   GL_UNSIGNED_BYTE, 0);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_, height_, format,
                      GL_UNSIGNED_BYTE, dst_pixels->Data());

    gl->GenMailboxCHROMIUM(frame_resource->mailbox_.name);
    gl->ProduceTextureCHROMIUM(GL_TEXTURE_2D, frame_resource->mailbox_.name);
  }

  const GLuint64 fence_sync = gl->InsertFenceSyncCHROMIUM();
  gl->Flush();
  gpu::SyncToken sync_token;
  gl->GenSyncTokenCHROMIUM(fence_sync, sync_token.GetData());

  resource.mailbox_holder =
      gpu::MailboxHolder(frame_resource->mailbox_, sync_token, GL_TEXTURE_2D);
  resource.read_lock_fences_enabled = false;
  resource.is_software = false;

  resources_.insert(next_resource_id_, std::move(frame_resource));
  gr->resetContext(kTextureBinding_GrGLBackendState);
}

void OffscreenCanvasResourceProvider::
    SetTransferableResourceToStaticBitmapImage(
        viz::TransferableResource& resource,
        RefPtr<StaticBitmapImage> image) {
  DCHECK(image->IsTextureBacked());
  DCHECK(image->IsValid());
  image->EnsureMailbox(kVerifiedSyncToken);
  resource.mailbox_holder = gpu::MailboxHolder(
      image->GetMailbox(), image->GetSyncToken(), GL_TEXTURE_2D);
  resource.read_lock_fences_enabled = false;
  resource.is_software = false;

  std::unique_ptr<FrameResource> frame_resource =
      CreateOrRecycleFrameResource();
  frame_resource->image_ = std::move(image);
  resources_.insert(next_resource_id_, std::move(frame_resource));
}

OffscreenCanvasResourceProvider::FrameResource::~FrameResource() {
  if (context_provider_wrapper_) {
    gpu::gles2::GLES2Interface* gl =
        context_provider_wrapper_->ContextProvider()->ContextGL();
    if (texture_id_)
      gl->DeleteTextures(1, &texture_id_);
    if (image_id_)
      gl->DestroyImageCHROMIUM(image_id_);
  }
}

void OffscreenCanvasResourceProvider::ReclaimResources(
    const WTF::Vector<viz::ReturnedResource>& resources) {
  for (const auto& resource : resources) {
    auto it = resources_.find(resource.id);

    DCHECK(it != resources_.end());
    if (it == resources_.end())
      continue;

    if (it->value->context_provider_wrapper_ && resource.sync_token.HasData()) {
      it->value->context_provider_wrapper_->ContextProvider()
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
    recycleable_resource_ = std::move(it->value);
    // release SkImage immediately since it is not recycleable
    recycleable_resource_->image_ = nullptr;
    resources_.erase(it);
  }
}

}  // namespace blink
