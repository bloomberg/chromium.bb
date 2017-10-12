// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/AcceleratedStaticBitmapImage.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WebTaskRunner.h"
#include "platform/graphics/MailboxTextureHolder.h"
#include "platform/graphics/SkiaTextureHolder.h"
#include "platform/graphics/gpu/SharedGpuContext.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/gpu/GrTexture.h"

#include <memory>
#include <utility>

namespace blink {

RefPtr<AcceleratedStaticBitmapImage>
AcceleratedStaticBitmapImage::CreateFromSkImage(
    sk_sp<SkImage> image,
    WeakPtr<WebGraphicsContext3DProviderWrapper>&& context_provider_wrapper) {
  DCHECK(image->isTextureBacked());
  return WTF::AdoptRef(new AcceleratedStaticBitmapImage(
      std::move(image), std::move(context_provider_wrapper)));
}

RefPtr<AcceleratedStaticBitmapImage>
AcceleratedStaticBitmapImage::CreateFromWebGLContextImage(
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& sync_token,
    unsigned texture_id,
    WeakPtr<WebGraphicsContext3DProviderWrapper>&& context_provider_wrapper,
    IntSize mailbox_size) {
  return WTF::AdoptRef(new AcceleratedStaticBitmapImage(
      mailbox, sync_token, texture_id, std::move(context_provider_wrapper),
      mailbox_size));
}

AcceleratedStaticBitmapImage::AcceleratedStaticBitmapImage(
    sk_sp<SkImage> image,
    WeakPtr<WebGraphicsContext3DProviderWrapper>&& context_provider_wrapper) {
  texture_holder_ = WTF::WrapUnique(new SkiaTextureHolder(
      std::move(image), std::move(context_provider_wrapper)));
  thread_checker_.DetachFromThread();
}

AcceleratedStaticBitmapImage::AcceleratedStaticBitmapImage(
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& sync_token,
    unsigned texture_id,
    WeakPtr<WebGraphicsContext3DProviderWrapper>&& context_provider_wrapper,
    IntSize mailbox_size) {
  texture_holder_ = WTF::WrapUnique(new MailboxTextureHolder(
      mailbox, sync_token, texture_id, std::move(context_provider_wrapper),
      mailbox_size));
  thread_checker_.DetachFromThread();
}

namespace {

void DestroySkImageOnOriginalThread(
    sk_sp<SkImage> image,
    WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    std::unique_ptr<gpu::SyncToken> sync_token) {
  if (context_provider_wrapper &&
      image->isValid(
          context_provider_wrapper->ContextProvider()->GetGrContext())) {
    if (sync_token->HasData()) {
      // To make sure skia does not recycle the texture while it is still in use
      // by another context.
      context_provider_wrapper->ContextProvider()
          ->ContextGL()
          ->WaitSyncTokenCHROMIUM(sync_token->GetData());
    }
    // In case texture was used by compositor, which may have changed params.
    image->getTexture()->textureParamsModified();
  }
  // destroy by letting |image| go out of scope
}

}  // unnamed namespace

AcceleratedStaticBitmapImage::~AcceleratedStaticBitmapImage() {
  // If the original SkImage was retained, it must be destroyed on the thread
  // where it came from. In the same thread case, there is nothing to do because
  // the regular destruction flow is fine.
  if (original_skia_image_) {
    std::unique_ptr<gpu::SyncToken> sync_token =
        base::WrapUnique(new gpu::SyncToken(texture_holder_->GetSyncToken()));
    if (original_skia_image_thread_id_ !=
        Platform::Current()->CurrentThread()->ThreadId()) {
      original_skia_image_task_runner_->PostTask(
          BLINK_FROM_HERE,
          CrossThreadBind(
              &DestroySkImageOnOriginalThread, std::move(original_skia_image_),
              std::move(original_skia_image_context_provider_wrapper_),
              WTF::Passed(std::move(sync_token))));
    } else {
      DestroySkImageOnOriginalThread(
          std::move(original_skia_image_),
          std::move(original_skia_image_context_provider_wrapper_),
          std::move(sync_token));
    }
  }
}

void AcceleratedStaticBitmapImage::RetainOriginalSkImageForCopyOnWrite() {
  DCHECK(texture_holder_->IsSkiaTextureHolder());
  original_skia_image_ = texture_holder_->GetSkImage();
  original_skia_image_context_provider_wrapper_ = ContextProviderWrapper();
  DCHECK(original_skia_image_);
  WebThread* thread = Platform::Current()->CurrentThread();
  original_skia_image_thread_id_ = thread->ThreadId();
  original_skia_image_task_runner_ = thread->GetWebTaskRunner();
}

IntSize AcceleratedStaticBitmapImage::Size() const {
  return texture_holder_->Size();
}

RefPtr<StaticBitmapImage> AcceleratedStaticBitmapImage::MakeUnaccelerated() {
  CreateImageFromMailboxIfNeeded();
  return StaticBitmapImage::Create(
      texture_holder_->GetSkImage()->makeNonTextureImage());
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

  // TODO(junov) : could reduce overhead by using kOrderingBarrier when we know
  // that the source and destination context or on the same stream.
  EnsureMailbox(kUnverifiedSyncToken);

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

PaintImage AcceleratedStaticBitmapImage::PaintImageForCurrentFrame() {
  // TODO(ccameron): This function should not ignore |colorBehavior|.
  // https://crbug.com/672306
  CheckThread();
  if (!IsValid())
    return PaintImage();
  CreateImageFromMailboxIfNeeded();

  return CreatePaintImageBuilder()
      .set_image(texture_holder_->GetSkImage())
      .set_completion_state(PaintImage::CompletionState::DONE)
      .TakePaintImage();
}

void AcceleratedStaticBitmapImage::Draw(PaintCanvas* canvas,
                                        const PaintFlags& flags,
                                        const FloatRect& dst_rect,
                                        const FloatRect& src_rect,
                                        RespectImageOrientationEnum,
                                        ImageClampingMode image_clamping_mode,
                                        ImageDecodingMode decode_mode) {
  auto paint_image = PaintImageForCurrentFrame();
  if (!paint_image)
    return;
  auto paint_image_decoding_mode = ToPaintImageDecodingMode(decode_mode);
  if (paint_image.decoding_mode() != paint_image_decoding_mode) {
    paint_image = PaintImageBuilder::WithCopy(std::move(paint_image))
                      .set_decoding_mode(paint_image_decoding_mode)
                      .TakePaintImage();
  }
  StaticBitmapImage::DrawHelper(canvas, flags, dst_rect, src_rect,
                                image_clamping_mode, paint_image);
}

bool AcceleratedStaticBitmapImage::IsValid() const {
  return texture_holder_ && texture_holder_->IsValid();
}

WebGraphicsContext3DProvider* AcceleratedStaticBitmapImage::ContextProvider()
    const {
  if (!IsValid())
    return nullptr;
  return texture_holder_->ContextProvider();
}

WeakPtr<WebGraphicsContext3DProviderWrapper>
AcceleratedStaticBitmapImage::ContextProviderWrapper() const {
  if (!IsValid())
    return nullptr;
  return texture_holder_->ContextProviderWrapper();
}

void AcceleratedStaticBitmapImage::CreateImageFromMailboxIfNeeded() {
  if (texture_holder_->IsSkiaTextureHolder())
    return;
  texture_holder_ =
      WTF::WrapUnique(new SkiaTextureHolder(std::move(texture_holder_)));
}

void AcceleratedStaticBitmapImage::EnsureMailbox(MailboxSyncMode mode) {
  if (!texture_holder_->IsMailboxTextureHolder()) {
    if (!original_skia_image_) {
      // To ensure that the texture resource stays alive we only really need
      // to retain the source SkImage until the mailbox is consumed, but this
      // works too.
      RetainOriginalSkImageForCopyOnWrite();
    }

    texture_holder_ =
        WTF::WrapUnique(new MailboxTextureHolder(std::move(texture_holder_)));
  }
  texture_holder_->Sync(mode);
}

void AcceleratedStaticBitmapImage::Transfer() {
  CheckThread();
  EnsureMailbox(kUnverifiedSyncToken);
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

void AcceleratedStaticBitmapImage::Abandon() {
  texture_holder_->Abandon();
}

}  // namespace blink
