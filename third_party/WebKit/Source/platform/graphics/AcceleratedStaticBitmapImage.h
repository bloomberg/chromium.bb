// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AcceleratedStaticBitmapImage_h
#define AcceleratedStaticBitmapImage_h

#include "base/threading/thread_checker.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/TextureHolder.h"
#include "platform/wtf/WeakPtr.h"

#include <memory>

class GrContext;

namespace blink {
class WebGraphicsContext3DProviderWrapper;
class TextureHolder;

class PLATFORM_EXPORT AcceleratedStaticBitmapImage final
    : public StaticBitmapImage {
 public:
  ~AcceleratedStaticBitmapImage() override;
  // SkImage with a texture backing that is assumed to be from the shared
  // context of the current thread.
  static PassRefPtr<AcceleratedStaticBitmapImage> CreateFromSharedContextImage(
      sk_sp<SkImage>);
  // Can specify the GrContext that created the texture backing. Ideally all
  // callers would use this option. The |mailbox| is a name for the texture
  // backing, allowing other contexts to use the same backing.
  static PassRefPtr<AcceleratedStaticBitmapImage> CreateFromWebGLContextImage(
      const gpu::Mailbox&,
      const gpu::SyncToken&,
      unsigned texture_id,
      WeakPtr<WebGraphicsContext3DProviderWrapper>,
      IntSize mailbox_size);

  bool CurrentFrameKnownToBeOpaque(MetadataMode = kUseCurrentMetadata) override;
  IntSize Size() const override;
  sk_sp<SkImage> ImageForCurrentFrame() override;
  bool IsTextureBacked() const override { return true; }

  void Draw(PaintCanvas*,
            const PaintFlags&,
            const FloatRect& dst_rect,
            const FloatRect& src_rect,
            RespectImageOrientationEnum,
            ImageClampingMode) override;

  void CopyToTexture(WebGraphicsContext3DProvider*,
                     GLenum dest_target,
                     GLuint dest_texture_id,
                     bool flip_y,
                     const IntPoint& dest_point,
                     const IntRect& source_sub_rectangle) override;

  bool HasMailbox() final { return texture_holder_->IsMailboxTextureHolder(); }
  // To be called on sender thread before performing a transfer
  void Transfer() final;

  void EnsureMailbox() final;
  gpu::Mailbox GetMailbox() final { return texture_holder_->GetMailbox(); }
  gpu::SyncToken GetSyncToken() final {
    return texture_holder_->GetSyncToken();
  }
  void UpdateSyncToken(gpu::SyncToken) final;

 private:
  AcceleratedStaticBitmapImage(sk_sp<SkImage>);
  AcceleratedStaticBitmapImage(const gpu::Mailbox&,
                               const gpu::SyncToken&,
                               unsigned texture_id,
                               WeakPtr<WebGraphicsContext3DProviderWrapper>,
                               IntSize mailbox_size);

  void CreateImageFromMailboxIfNeeded();
  void CheckThread();
  void WaitSyncTokenIfNeeded();
  bool IsValid();

  std::unique_ptr<TextureHolder> texture_holder_;

  base::ThreadChecker thread_checker_;
  bool detach_thread_at_next_check_ = false;
};

}  // namespace blink

#endif
