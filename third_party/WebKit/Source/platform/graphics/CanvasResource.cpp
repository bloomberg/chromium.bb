// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CanvasResource.h"

#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "platform/graphics/CanvasResourceProvider.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/gpu/SharedGpuContext.h"
#include "public/platform/Platform.h"
#include "skia/ext/texture_handle.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gfx/color_space.h"

namespace blink {

CanvasResource::CanvasResource(base::WeakPtr<CanvasResourceProvider> provider,
                               SkFilterQuality filter_quality)
    : provider_(std::move(provider)), filter_quality_(filter_quality) {}

CanvasResource::~CanvasResource() {
  // Sync token should have been waited on in sub-class implementation of
  // Abandon().
  DCHECK(!sync_token_for_release_.HasData());
}

gpu::gles2::GLES2Interface* CanvasResource::ContextGL() const {
  if (!ContextProviderWrapper())
    return nullptr;
  return ContextProviderWrapper()->ContextProvider()->ContextGL();
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

bool CanvasResource::PrepareTransferableResource(
    viz::TransferableResource* out_resource,
    std::unique_ptr<viz::SingleReleaseCallback>* out_callback) {
  DCHECK(IsValid());

  // This should never be called on unaccelerated canvases (for now).

  // Gpu compositing is a prerequisite for accelerated 2D canvas
  // TODO: For WebGL to use this, we must add software composing support.
  DCHECK(SharedGpuContext::IsGpuCompositingEnabled());
  auto gl = ContextGL();
  DCHECK(gl);

  const gpu::Mailbox& mailbox = GetOrCreateGpuMailbox();
  if (mailbox.IsZero())
    return false;

  *out_resource = viz::TransferableResource::MakeGLOverlay(
      mailbox, GLFilter(), TextureTarget(), GetSyncToken(), gfx::Size(Size()),
      IsOverlayCandidate());

  scoped_refptr<CanvasResource> this_ref(this);
  auto func = WTF::Bind(&ReleaseFrameResources, provider_,
                        WTF::Passed(std::move(this_ref)));
  *out_callback = viz::SingleReleaseCallback::Create(std::move(func));
  return true;
}

GrContext* CanvasResource::GetGrContext() const {
  if (!ContextProviderWrapper())
    return nullptr;
  return ContextProviderWrapper()->ContextProvider()->GetGrContext();
}

GLenum CanvasResource::GLFilter() const {
  return filter_quality_ == kNone_SkFilterQuality ? GL_NEAREST : GL_LINEAR;
}

// CanvasResource_Bitmap
//==============================================================================

CanvasResource_Bitmap::CanvasResource_Bitmap(
    scoped_refptr<StaticBitmapImage> image,
    base::WeakPtr<CanvasResourceProvider> provider,
    SkFilterQuality filter_quality)
    : CanvasResource(std::move(provider), filter_quality),
      image_(std::move(image)) {}

scoped_refptr<CanvasResource_Bitmap> CanvasResource_Bitmap::Create(
    scoped_refptr<StaticBitmapImage> image,
    base::WeakPtr<CanvasResourceProvider> provider,
    SkFilterQuality filter_quality) {
  scoped_refptr<CanvasResource_Bitmap> resource =
      AdoptRef(new CanvasResource_Bitmap(std::move(image), std::move(provider),
                                         filter_quality));
  if (resource->IsValid())
    return resource;
  return nullptr;
}

bool CanvasResource_Bitmap::IsValid() const {
  if (!image_)
    return false;
  return image_->IsValid();
}

void CanvasResource_Bitmap::TearDown() {
  WaitSyncTokenBeforeRelease();
  auto gl = ContextGL();
  if (gl && HasGpuMailbox()) {
    DCHECK(image_->IsTextureBacked());
    // To avoid leaking Mailbox records, we must disassociate the mailbox
    // before image_ goes out of scope because skia might recycle the texture.
    gl->ProduceTextureDirectCHROMIUM(0, GetOrCreateGpuMailbox().name);
  }
  image_ = nullptr;
}

IntSize CanvasResource_Bitmap::Size() const {
  if (!image_)
    return IntSize(0, 0);
  return IntSize(image_->width(), image_->height());
}

GLenum CanvasResource_Bitmap::TextureTarget() const {
  return GL_TEXTURE_2D;
}

const gpu::Mailbox& CanvasResource_Bitmap::GetOrCreateGpuMailbox() {
  DCHECK(image_);  // Calling code should check IsValid() before calling this.
  image_->EnsureMailbox(kUnverifiedSyncToken, GLFilter());
  return image_->GetMailbox();
}

bool CanvasResource_Bitmap::HasGpuMailbox() const {
  return image_ && image_->HasMailbox();
}

const gpu::SyncToken& CanvasResource_Bitmap::GetSyncToken() const {
  DCHECK(image_);  // Calling code should check IsValid() before calling this.
  return image_->GetSyncToken();
}

base::WeakPtr<WebGraphicsContext3DProviderWrapper>
CanvasResource_Bitmap::ContextProviderWrapper() const {
  if (!image_)
    return nullptr;
  return image_->ContextProviderWrapper();
}

// CanvasResource_GpuMemoryBuffer
//==============================================================================

CanvasResource_GpuMemoryBuffer::CanvasResource_GpuMemoryBuffer(
    const IntSize& size,
    const CanvasColorParams& color_params,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceProvider> provider,
    SkFilterQuality filter_quality)
    : CanvasResource(provider, filter_quality),
      context_provider_wrapper_(std::move(context_provider_wrapper)),
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
      AdoptRef(new CanvasResource_GpuMemoryBuffer(
          size, color_params, std::move(context_provider_wrapper), provider,
          filter_quality));
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
    GLenum target = TextureTarget();
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

const gpu::Mailbox& CanvasResource_GpuMemoryBuffer::GetOrCreateGpuMailbox() {
  auto gl = ContextGL();
  DCHECK(gl);  // caller should already have early exited if !gl.
  if (gpu_mailbox_.IsZero() && gl) {
    gl->GenMailboxCHROMIUM(gpu_mailbox_.name);
    gl->ProduceTextureDirectCHROMIUM(texture_id_, gpu_mailbox_.name);
  }
  if (mailbox_needs_new_sync_token_) {
    mailbox_needs_new_sync_token_ = false;
    gl->GenUnverifiedSyncTokenCHROMIUM(sync_token_.GetData());
  }
  return gpu_mailbox_;
}

bool CanvasResource_GpuMemoryBuffer::HasGpuMailbox() const {
  return !gpu_mailbox_.IsZero();
}

const gpu::SyncToken& CanvasResource_GpuMemoryBuffer::GetSyncToken() const {
  return sync_token_;
}

void CanvasResource_GpuMemoryBuffer::CopyFromTexture(GLuint source_texture,
                                                     GLenum format,
                                                     GLenum type) {
  if (!IsValid())
    return;

  ContextGL()->CopyTextureCHROMIUM(
      source_texture, 0 /*sourceLevel*/, TextureTarget(), texture_id_,
      0 /*destLevel*/, format, type, false /*unpackFlipY*/,
      false /*unpackPremultiplyAlpha*/, false /*unpackUnmultiplyAlpha*/);
  mailbox_needs_new_sync_token_ = true;
}

base::WeakPtr<WebGraphicsContext3DProviderWrapper>
CanvasResource_GpuMemoryBuffer::ContextProviderWrapper() const {
  return context_provider_wrapper_;
}

}  // namespace blink
