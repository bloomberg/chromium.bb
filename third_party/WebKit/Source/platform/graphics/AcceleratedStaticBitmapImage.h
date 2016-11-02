// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AcceleratedStaticBitmapImage_h
#define AcceleratedStaticBitmapImage_h

#include "base/threading/thread_checker.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"

#include <memory>

class GrContext;

namespace blink {
class WebThread;

class PLATFORM_EXPORT AcceleratedStaticBitmapImage final
    : public StaticBitmapImage {
 public:
  // SkImage with a texture backing that is assumed to be from the shared
  // context of the current thread.
  static PassRefPtr<AcceleratedStaticBitmapImage> createFromSharedContextImage(
      sk_sp<SkImage>);
  // Can specify the GrContext that created the texture backing the for the
  // given SkImage. Ideally all callers would use this option. The |mailbox| is
  // a name for the texture backing the SkImage, allowing other contexts to use
  // the same backing.
  static PassRefPtr<AcceleratedStaticBitmapImage> createFromWebGLContextImage(
      sk_sp<SkImage>,
      const gpu::Mailbox&,
      const gpu::SyncToken&);

  ~AcceleratedStaticBitmapImage() override;

  // StaticBitmapImage overrides.
  sk_sp<SkImage> imageForCurrentFrame() override;
  void copyToTexture(WebGraphicsContext3DProvider*,
                     GLuint destTextureId,
                     GLenum destInternalFormat,
                     GLenum destType,
                     bool flipY) override;

  // Image overrides.
  void draw(SkCanvas*,
            const SkPaint&,
            const FloatRect& dstRect,
            const FloatRect& srcRect,
            RespectImageOrientationEnum,
            ImageClampingMode) override;

  // To be called on sender thread before performing a transfer
  void transfer() final;

  void ensureMailbox() final;
  gpu::Mailbox getMailbox() final { return m_mailbox; }
  gpu::SyncToken getSyncToken() final { return m_syncToken; }

 private:
  AcceleratedStaticBitmapImage(sk_sp<SkImage>);
  AcceleratedStaticBitmapImage(sk_sp<SkImage>,
                               const gpu::Mailbox&,
                               const gpu::SyncToken&);

  void createImageFromMailboxIfNeeded();
  void checkThread();
  void waitSyncTokenIfNeeded();
  bool isValid();
  void releaseImageThreadSafe();
  bool imageBelongsToSharedContext();

  // Id of the shared context where m_image was created
  unsigned m_sharedContextId;
  // True when the below mailbox and sync token are valid for getting at the
  // texture backing the object's SkImage.
  bool m_hasMailbox = false;
  // A mailbox referring to the texture id backing the SkImage. The mailbox is
  // valid as long as the SkImage is held alive.
  gpu::Mailbox m_mailbox;
  // This token must be waited for before using the mailbox.
  gpu::SyncToken m_syncToken;
  // Thread that |m_image| belongs to. Set to null when |m_image| belongs to the
  // same thread as this AcceleratedStaticBitmapImage.  Should only be non-null
  // after a transfer.
  WebThread* m_imageThread = nullptr;

  base::ThreadChecker m_threadChecker;
  bool m_detachThreadAtNextCheck = false;
};

}  // namespace blink

#endif
