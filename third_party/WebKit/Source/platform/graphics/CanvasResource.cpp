// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CanvasResource.h"

#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "public/platform/Platform.h"
#include "skia/ext/texture_handle.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gfx/color_space.h"

namespace blink {

CanvasResource::CanvasResource(
    WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper)
    : context_provider_wrapper_(std::move(context_provider_wrapper)) {}

CanvasResource::~CanvasResource() {
  // Sync token should have been waited on in sub-class implementation of
  // Abandon().
  DCHECK(!sync_token_for_release_.HasData());
}

gpu::gles2::GLES2Interface* CanvasResource::ContextGL() const {
  if (!context_provider_wrapper_)
    return nullptr;
  return context_provider_wrapper_->ContextProvider()->ContextGL();
}

const gpu::Mailbox& CanvasResource::GpuMailbox() {
  if (gpu_mailbox_.IsZero()) {
    auto gl = ContextGL();
    DCHECK(gl);  // caller should already have early exited if !gl.
    if (gl) {
      gl->GenMailboxCHROMIUM(gpu_mailbox_.name);
    }
  }
  return gpu_mailbox_;
}

void CanvasResource::SetSyncTokenForRelease(const gpu::SyncToken& token) {
  sync_token_for_release_ = token;
}

void CanvasResource::WaitSyncTokenBeforeRelease() {
  auto gl = ContextGL();
  if (sync_token_for_release_.HasData() && gl) {
    gl->WaitSyncTokenCHROMIUM(sync_token_for_release_.GetData());
  }
  sync_token_for_release_.Clear();
}

// CanvasResource_Skia
//==============================================================================

CanvasResource_Skia::CanvasResource_Skia(
    sk_sp<SkImage> image,
    WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper)
    : CanvasResource(std::move(context_provider_wrapper)),
      image_(std::move(image)) {}

std::unique_ptr<CanvasResource_Skia> CanvasResource_Skia::Create(
    sk_sp<SkImage> image,
    WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper) {
  std::unique_ptr<CanvasResource_Skia> resource =
      WTF::WrapUnique(new CanvasResource_Skia(
          std::move(image), std::move(context_provider_wrapper)));
  if (resource->IsValid())
    return resource;
  return nullptr;
}

bool CanvasResource_Skia::IsValid() const {
  if (!image_)
    return false;
  if (!image_->isTextureBacked())
    return true;
  return !!context_provider_wrapper_;
}

void CanvasResource_Skia::Abandon() {
  WaitSyncTokenBeforeRelease();
  image_ = nullptr;
  context_provider_wrapper_ = nullptr;
}

GLuint CanvasResource_Skia::TextureId() const {
  DCHECK(image_->isTextureBacked());
  return skia::GrBackendObjectToGrGLTextureInfo(image_->getTextureHandle(true))
      ->fID;
}

// CanvasResource_GpuMemoryBuffer
//==============================================================================

CanvasResource_GpuMemoryBuffer::CanvasResource_GpuMemoryBuffer(
    const IntSize& size,
    const CanvasColorParams& color_params,
    WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper)
    : CanvasResource(std::move(context_provider_wrapper)),
      color_params_(color_params) {
  if (!context_provider_wrapper_)
    return;
  auto gl = context_provider_wrapper_->ContextProvider()->ContextGL();
  auto gr = context_provider_wrapper_->ContextProvider()->GetGrContext();
  if (!gl || !gr)
    return;
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager =
      Platform::Current()->GetGpuMemoryBufferManager();
  if (!gpu_memory_buffer_manager)
    return;
  gpu_memory_buffer_ = gpu_memory_buffer_manager->CreateGpuMemoryBuffer(
      gfx::Size(size.Width(), size.Height()),
      color_params_.GetBufferFormat(),  // Use format
      gfx::BufferUsage::SCANOUT, gpu::kNullSurfaceHandle);
  if (!gpu_memory_buffer_) {
    return;
  }
  image_id_ = gl->CreateImageCHROMIUM(gpu_memory_buffer_->AsClientBuffer(),
                                      size.Width(), size.Height(),
                                      color_params_.GLInternalFormat());
  if (!image_id_) {
    gpu_memory_buffer_ = nullptr;
    return;
  }

  gpu_memory_buffer_->SetColorSpace(color_params.GetStorageGfxColorSpace());
  gl->GenTextures(1, &texture_id_);
  const GLenum target = GL_TEXTURE_RECTANGLE_ARB;
  gl->BindTexture(target, texture_id_);
  gl->BindTexImage2DCHROMIUM(target, image_id_);
  gr->resetContext(kTextureBinding_GrGLBackendState);
}

std::unique_ptr<CanvasResource_GpuMemoryBuffer>
CanvasResource_GpuMemoryBuffer::Create(
    const IntSize& size,
    const CanvasColorParams& color_params,
    WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper) {
  std::unique_ptr<CanvasResource_GpuMemoryBuffer> resource =
      WTF::WrapUnique(new CanvasResource_GpuMemoryBuffer(
          size, color_params, context_provider_wrapper));
  if (resource->IsValid())
    return resource;
  return nullptr;
}

void CanvasResource_GpuMemoryBuffer::Abandon() {
  WaitSyncTokenBeforeRelease();
  if (!context_provider_wrapper_ || !image_id_)
    return;
  auto gl = context_provider_wrapper_->ContextProvider()->ContextGL();
  auto gr = context_provider_wrapper_->ContextProvider()->GetGrContext();
  if (gl && texture_id_) {
    GLenum target = GL_TEXTURE_RECTANGLE_ARB;
    gl->BindTexture(target, texture_id_);
    gl->ReleaseTexImage2DCHROMIUM(target, image_id_);
    gl->DestroyImageCHROMIUM(image_id_);
    gl->DeleteTextures(1, &texture_id_);
    gl->BindTexture(target, 0);
    if (gr) {
      gr->resetContext(kTextureBinding_GrGLBackendState);
    }
  }
  image_id_ = 0;
  texture_id_ = 0;
  gpu_memory_buffer_ = nullptr;
}

}  // namespace blink
