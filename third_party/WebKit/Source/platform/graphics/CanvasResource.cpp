// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CanvasResource.h"

#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "platform/graphics/CanvasResourceProvider.h"
#include "platform/graphics/gpu/SharedGpuContext.h"
#include "public/platform/Platform.h"
#include "skia/ext/texture_handle.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gfx/color_space.h"

namespace blink {

CanvasResource::CanvasResource(
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceProvider> provider,
    SkFilterQuality filter_quality)
    : context_provider_wrapper_(std::move(context_provider_wrapper)),
      provider_(std::move(provider)),
      filter_quality_(filter_quality) {}

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

const gpu::Mailbox& CanvasResource::GetOrCreateGpuMailbox() {
  if (gpu_mailbox_.IsZero()) {
    auto gl = ContextGL();
    DCHECK(gl);  // caller should already have early exited if !gl.
    if (gl) {
      gl->GenMailboxCHROMIUM(gpu_mailbox_.name);
    }
  }
  return gpu_mailbox_;
}

bool CanvasResource::HasGpuMailbox() const {
  return !gpu_mailbox_.IsZero();
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

static void ReleaseFrameResources(
    base::WeakPtr<CanvasResourceProvider> resource_provider,
    scoped_refptr<CanvasResource> resource,
    const gpu::SyncToken& sync_token,
    bool lost_resource) {
  resource->SetSyncTokenForRelease(sync_token);
  if (lost_resource) {
    resource->Abandon();
  }
  if (resource_provider && !lost_resource && resource->IsRecycleable()) {
    resource_provider->RecycleResource(std::move(resource));
  }
}

void CanvasResource::PrepareTransferableResource(
    viz::TransferableResource* out_resource,
    std::unique_ptr<viz::SingleReleaseCallback>* out_callback) {
  // This should never be called on unaccelerated canvases (for now).
  DCHECK(TextureId());

  // Gpu compositing is a prerequisite for accelerated 2D canvas
  // TODO: For WebGL to use this, we must add software composing support.
  DCHECK(SharedGpuContext::IsGpuCompositingEnabled());
  auto gl = ContextGL();
  DCHECK(gl);

  GLuint filter =
      filter_quality_ == kNone_SkFilterQuality ? GL_NEAREST : GL_LINEAR;
  GLenum target = TextureTarget();
  GLint texture_id = TextureId();

  gl->BindTexture(target, texture_id);
  gl->TexParameteri(target, GL_TEXTURE_MAG_FILTER, filter);
  gl->TexParameteri(target, GL_TEXTURE_MIN_FILTER, filter);
  gl->TexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl->TexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  gl->ProduceTextureDirectCHROMIUM(texture_id, GetOrCreateGpuMailbox().name);
  const GLuint64 fence_sync = gl->InsertFenceSyncCHROMIUM();
  gl->ShallowFlushCHROMIUM();
  gpu::SyncToken sync_token;
  gl->GenUnverifiedSyncTokenCHROMIUM(fence_sync, sync_token.GetData());

  *out_resource = viz::TransferableResource::MakeGLOverlay(
      GetOrCreateGpuMailbox(), filter, target, sync_token, gfx::Size(Size()),
      IsOverlayCandidate());

  gl->BindTexture(target, 0);

  // Because we are changing the texture binding without going through skia,
  // we must dirty the context.
  GrContext* gr = GetGrContext();
  if (gr)
    gr->resetContext(kTextureBinding_GrGLBackendState);

  scoped_refptr<CanvasResource> this_ref(this);
  auto func = WTF::Bind(&ReleaseFrameResources, provider_,
                        WTF::Passed(std::move(this_ref)));
  *out_callback = viz::SingleReleaseCallback::Create(std::move(func));
}

GrContext* CanvasResource::GetGrContext() const {
  if (!context_provider_wrapper_)
    return nullptr;
  return context_provider_wrapper_->ContextProvider()->GetGrContext();
}

// CanvasResource_Skia
//==============================================================================

CanvasResource_Skia::CanvasResource_Skia(
    sk_sp<SkImage> image,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceProvider> provider,
    SkFilterQuality filter_quality)
    : CanvasResource(std::move(context_provider_wrapper),
                     std::move(provider),
                     filter_quality),
      image_(std::move(image)) {}

scoped_refptr<CanvasResource_Skia> CanvasResource_Skia::Create(
    sk_sp<SkImage> image,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceProvider> provider,
    SkFilterQuality filter_quality) {
  scoped_refptr<CanvasResource_Skia> resource =
      AdoptRef(new CanvasResource_Skia(std::move(image),
                                       std::move(context_provider_wrapper),
                                       std::move(provider), filter_quality));
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

void CanvasResource_Skia::TearDown() {
  WaitSyncTokenBeforeRelease();
  auto gl = ContextGL();
  if (gl && HasGpuMailbox()) {
    DCHECK(image_->isTextureBacked());
    // To avoid leaking Mailbox records, we must disassociate the mailbox
    // before image_ goes out of scope because skia might recycle the texture.
    gl->ProduceTextureDirectCHROMIUM(0, GpuMailbox().name);
  }
  image_ = nullptr;
  context_provider_wrapper_ = nullptr;
}

IntSize CanvasResource_Skia::Size() const {
  return IntSize(image_->width(), image_->height());
}

GLuint CanvasResource_Skia::TextureId() const {
  DCHECK(image_->isTextureBacked());
  return skia::GrBackendObjectToGrGLTextureInfo(image_->getTextureHandle(true))
      ->fID;
}

GLenum CanvasResource_Skia::TextureTarget() const {
  return GL_TEXTURE_2D;
}

void CanvasResource_Skia::PrepareTransferableResource(
    viz::TransferableResource* out_resource,
    std::unique_ptr<viz::SingleReleaseCallback>* out_callback) {
  DCHECK(image_->isTextureBacked());
  CanvasResource::PrepareTransferableResource(out_resource, out_callback);
  image_->getTexture()->textureParamsModified();
}

// CanvasResource_GpuMemoryBuffer
//==============================================================================

CanvasResource_GpuMemoryBuffer::CanvasResource_GpuMemoryBuffer(
    const IntSize& size,
    const CanvasColorParams& color_params,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceProvider> provider,
    SkFilterQuality filter_quality)
    : CanvasResource(std::move(context_provider_wrapper),
                     provider,
                     filter_quality),
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

CanvasResource_GpuMemoryBuffer::~CanvasResource_GpuMemoryBuffer() {
  TearDown();
}

GLenum CanvasResource_GpuMemoryBuffer::TextureTarget() const {
  return GL_TEXTURE_RECTANGLE_ARB;
}

IntSize CanvasResource_GpuMemoryBuffer::Size() const {
  return IntSize(gpu_memory_buffer_->GetSize().width(),
                 gpu_memory_buffer_->GetSize().height());
}

scoped_refptr<CanvasResource_GpuMemoryBuffer>
CanvasResource_GpuMemoryBuffer::Create(
    const IntSize& size,
    const CanvasColorParams& color_params,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceProvider> provider,
    SkFilterQuality filter_quality) {
  scoped_refptr<CanvasResource_GpuMemoryBuffer> resource =
      AdoptRef(new CanvasResource_GpuMemoryBuffer(size, color_params,
                                                  context_provider_wrapper,
                                                  provider, filter_quality));
  if (resource->IsValid())
    return resource;
  return nullptr;
}

void CanvasResource_GpuMemoryBuffer::TearDown() {
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
