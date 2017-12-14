// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AcceleratedStaticBitmapImage_h
#define AcceleratedStaticBitmapImage_h

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/TextureHolder.h"

#include <memory>

class GrContext;

namespace blink {
class WebGraphicsContext3DProviderWrapper;
class TextureHolder;

class PLATFORM_EXPORT AcceleratedStaticBitmapImage final
    : public StaticBitmapImage {
 public:
  ~AcceleratedStaticBitmapImage() override;
  // SkImage with a texture backing.
  static scoped_refptr<AcceleratedStaticBitmapImage> CreateFromSkImage(
      sk_sp<SkImage>,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>&&);
  // Can specify the GrContext that created the texture backing. Ideally all
  // callers would use this option. The |mailbox| is a name for the texture
  // backing, allowing other contexts to use the same backing.
  static scoped_refptr<AcceleratedStaticBitmapImage>
  CreateFromWebGLContextImage(
      const gpu::Mailbox&,
      const gpu::SyncToken&,
      unsigned texture_id,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>&&,
      IntSize mailbox_size);

  bool CurrentFrameKnownToBeOpaque(MetadataMode = kUseCurrentMetadata) override;
  IntSize Size() const override;
  bool IsTextureBacked() const override { return true; }
  scoped_refptr<StaticBitmapImage> MakeAccelerated(
      base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_wrapper)
      override {
    NOTREACHED();  // IsTextureBacked() is already true.
    return nullptr;
  }

  void Draw(PaintCanvas*,
            const PaintFlags&,
            const FloatRect& dst_rect,
            const FloatRect& src_rect,
            RespectImageOrientationEnum,
            ImageClampingMode,
            ImageDecodingMode) override;

  bool IsValid() const final;
  WebGraphicsContext3DProvider* ContextProvider() const final;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWrapper()
      const final;
  scoped_refptr<StaticBitmapImage> MakeUnaccelerated() final;

  void CopyToTexture(WebGraphicsContext3DProvider*,
                     GLenum dest_target,
                     GLuint dest_texture_id,
                     bool flip_y,
                     const IntPoint& dest_point,
                     const IntRect& source_sub_rectangle) override;

  bool HasMailbox() const final {
    return texture_holder_->IsMailboxTextureHolder();
  }
  // To be called on sender thread before performing a transfer
  void Transfer() final;

  void EnsureMailbox(MailboxSyncMode, GLenum filter) final;

  const gpu::Mailbox& GetMailbox() const final {
    return texture_holder_->GetMailbox();
  }
  const gpu::SyncToken& GetSyncToken() const final {
    return texture_holder_->GetSyncToken();
  }
  void UpdateSyncToken(gpu::SyncToken) final;

  // Call this immediately after creation in cases where the source SkImage
  // was a snapshot of an SkSurface that may be rendered to after
  void RetainOriginalSkImageForCopyOnWrite();

  PaintImage PaintImageForCurrentFrame() override;

  void Abandon() final;

 private:
  AcceleratedStaticBitmapImage(
      sk_sp<SkImage>,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>&&);
  AcceleratedStaticBitmapImage(
      const gpu::Mailbox&,
      const gpu::SyncToken&,
      unsigned texture_id,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>&&,
      IntSize mailbox_size);

  void CreateImageFromMailboxIfNeeded();
  void CheckThread();
  void WaitSyncTokenIfNeeded();

  std::unique_ptr<TextureHolder> texture_holder_;

  base::ThreadChecker thread_checker_;
  bool detach_thread_at_next_check_ = false;

  // For RetainOriginalSkImageForCopyOnWrite()
  sk_sp<SkImage> original_skia_image_;
  scoped_refptr<WebTaskRunner> original_skia_image_task_runner_;
  PlatformThreadId original_skia_image_thread_id_;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper>
      original_skia_image_context_provider_wrapper_;
};

}  // namespace blink

#endif
