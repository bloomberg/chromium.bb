// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/AcceleratedStaticBitmapImage.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "platform/graphics/MailboxTextureHolder.h"
#include "platform/graphics/SkiaTextureHolder.h"
#include "platform/graphics/gpu/SharedGpuContext.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "third_party/skia/include/core/SkImage.h"

#include <memory>
#include <utility>

namespace blink {

PassRefPtr<AcceleratedStaticBitmapImage>
AcceleratedStaticBitmapImage::CreateFromSharedContextImage(
    sk_sp<SkImage> image) {
  return AdoptRef(new AcceleratedStaticBitmapImage(std::move(image)));
}

PassRefPtr<AcceleratedStaticBitmapImage>
AcceleratedStaticBitmapImage::CreateFromWebGLContextImage(
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& sync_token,
    unsigned texture_id,
    WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider,
    IntSize mailbox_size) {
  return AdoptRef(new AcceleratedStaticBitmapImage(
      mailbox, sync_token, texture_id, context_provider, mailbox_size));
}

AcceleratedStaticBitmapImage::AcceleratedStaticBitmapImage(
    sk_sp<SkImage> image) {
  texture_holder_ = WTF::WrapUnique(new SkiaTextureHolder(std::move(image)));
  thread_checker_.DetachFromThread();
}

AcceleratedStaticBitmapImage::AcceleratedStaticBitmapImage(
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& sync_token,
    unsigned texture_id,
    WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider,
    IntSize mailbox_size) {
  texture_holder_ = WTF::WrapUnique(new MailboxTextureHolder(
      mailbox, sync_token, texture_id, context_provider, mailbox_size));
  thread_checker_.DetachFromThread();
}

AcceleratedStaticBitmapImage::~AcceleratedStaticBitmapImage() {}

IntSize AcceleratedStaticBitmapImage::Size() const {
  return texture_holder_->Size();
}

void AcceleratedStaticBitmapImage::UpdateSyncToken(gpu::SyncToken sync_token) {
  texture_holder_->UpdateSyncToken(sync_token);
}

void AcceleratedStaticBitmapImage::CopyToTexture(
    WebGraphicsContext3DProvider* dest_provider,
    GLenum dest_target,
    GLuint dest_texture_id,
    bool flip_y,
    const IntPoint& dest_point,
    const IntRect& source_sub_rectangle) {
  CheckThread();
  if (!IsValid())
    return;
  // |destProvider| may not be the same context as the one used for |m_image|,
  // so we use a mailbox to generate a texture id for |destProvider| to access.
  EnsureMailbox();

  // Get a texture id that |destProvider| knows about and copy from it.
  gpu::gles2::GLES2Interface* dest_gl = dest_provider->ContextGL();
  dest_gl->WaitSyncTokenCHROMIUM(texture_holder_->GetSyncToken().GetData());
  GLuint source_texture_id = dest_gl->CreateAndConsumeTextureCHROMIUM(
      GL_TEXTURE_2D, texture_holder_->GetMailbox().name);
  dest_gl->CopySubTextureCHROMIUM(
      source_texture_id, 0, dest_target, dest_texture_id, 0, dest_point.X(),
      dest_point.Y(), source_sub_rectangle.X(), source_sub_rectangle.Y(),
      source_sub_rectangle.Width(), source_sub_rectangle.Height(), flip_y,
      false, false);
  // This drops the |destGL| context's reference on our |m_mailbox|, but it's
  // still held alive by our SkImage.
  dest_gl->DeleteTextures(1, &source_texture_id);
}

sk_sp<SkImage> AcceleratedStaticBitmapImage::ImageForCurrentFrame() {
  // TODO(ccameron): This function should not ignore |colorBehavior|.
  // https://crbug.com/672306
  CheckThread();
  if (!IsValid())
    return nullptr;
  CreateImageFromMailboxIfNeeded();
  return texture_holder_->GetSkImage();
}

void AcceleratedStaticBitmapImage::Draw(PaintCanvas* canvas,
                                        const PaintFlags& flags,
                                        const FloatRect& dst_rect,
                                        const FloatRect& src_rect,
                                        RespectImageOrientationEnum,
                                        ImageClampingMode image_clamping_mode) {
  const auto& paint_image = PaintImageForCurrentFrame();
  if (!paint_image)
    return;
  StaticBitmapImage::DrawHelper(canvas, flags, dst_rect, src_rect,
                                image_clamping_mode, paint_image);
}

bool AcceleratedStaticBitmapImage::IsValid() {
  if (!texture_holder_)
    return false;
  if (!SharedGpuContext::IsValid())
    return false;  // Gpu context was lost
  unsigned shared_context_id = texture_holder_->SharedContextId();
  if (shared_context_id != SharedGpuContext::kNoSharedContext &&
      shared_context_id != SharedGpuContext::ContextId()) {
    // Gpu context was lost and restored since the resource was created.
    return false;
  }
  return true;
}

void AcceleratedStaticBitmapImage::CreateImageFromMailboxIfNeeded() {
  if (texture_holder_->SharedContextId() != SharedGpuContext::kNoSharedContext)
    return;
  if (texture_holder_->IsSkiaTextureHolder())
    return;
  texture_holder_ =
      WTF::WrapUnique(new SkiaTextureHolder(std::move(texture_holder_)));
}

void AcceleratedStaticBitmapImage::EnsureMailbox() {
  if (texture_holder_->IsMailboxTextureHolder())
    return;

  texture_holder_ =
      WTF::WrapUnique(new MailboxTextureHolder(std::move(texture_holder_)));
}

void AcceleratedStaticBitmapImage::Transfer() {
  CheckThread();
  EnsureMailbox();
  // If |m_textureThreadTaskRunner| in TextureHolder is set, it means that
  // the |m_texture| in this class has been consumed on the current thread,
  // which may happen when we have chained transfers. When that is the case,
  // we must not reset |m_imageThreadTaskRunner|, so we ensure that
  // releaseImage() or releaseTexture() is called on the right thread.
  if (!texture_holder_->WasTransferred()) {
    WebThread* current_thread = Platform::Current()->CurrentThread();
    texture_holder_->SetWasTransferred(true);
    texture_holder_->SetTextureThreadTaskRunner(
        current_thread->GetWebTaskRunner());
  }
  detach_thread_at_next_check_ = true;
}

bool AcceleratedStaticBitmapImage::CurrentFrameKnownToBeOpaque(
    MetadataMode metadata_mode) {
  return texture_holder_->CurrentFrameKnownToBeOpaque(metadata_mode);
}

void AcceleratedStaticBitmapImage::CheckThread() {
  if (detach_thread_at_next_check_) {
    thread_checker_.DetachFromThread();
    detach_thread_at_next_check_ = false;
  }
  CHECK(thread_checker_.CalledOnValidThread());
}

}  // namespace blink
