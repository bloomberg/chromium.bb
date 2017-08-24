// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StaticBitmapImage_h
#define StaticBitmapImage_h

#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/Image.h"
#include "platform/wtf/WeakPtr.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

class WebGraphicsContext3DProvider;
class WebGraphicsContext3DProviderWrapper;

class PLATFORM_EXPORT StaticBitmapImage : public Image {
 public:
  // WebGraphicsContext3DProviderWrapper argument only needs to be provided if
  // The SkImage is texture backed, in which case it must be a reference to the
  // context provider that owns the GrContext with which the SkImage is
  // associated.
  static RefPtr<StaticBitmapImage> Create(
      sk_sp<SkImage>,
      WeakPtr<WebGraphicsContext3DProviderWrapper>&& = nullptr);
  static RefPtr<StaticBitmapImage> Create(PaintImage);

  bool IsStaticBitmapImage() const override { return true; }

  // Methods overridden by all sub-classes
  virtual ~StaticBitmapImage() {}

  // Methods have common implementation for all sub-classes
  bool CurrentFrameIsComplete() override { return true; }
  void DestroyDecodedData() {}

  // Methods that have a default implementation, and overridden by only one
  // sub-class
  virtual bool HasMailbox() const { return false; }
  virtual bool IsValid() const { return true; }
  virtual void Transfer() {}
  virtual void Abandon() {}
  // Creates a non-gpu copy of the image, or returns this if image is already
  // non-gpu.
  virtual RefPtr<StaticBitmapImage> MakeUnaccelerated() { return this; }

  // Methods overridden by AcceleratedStaticBitmapImage only
  virtual void CopyToTexture(WebGraphicsContext3DProvider*,
                             GLenum,
                             GLuint,
                             bool,
                             const IntPoint&,
                             const IntRect&) {
    NOTREACHED();
  }

  // EnsureMailbox modifies the internal state of an accelerated static bitmap
  // image to make sure that it is represented by a Mailbox.  This must be
  // called whenever the image is intende to be used in a differen GPU context.
  // It is important to use the correct MailboxSyncMode in order to achieve
  // optimal performance without compromising security or causeing graphics
  // glitches.  Here is how to select the aprropriate mode:
  //
  // Case 1: Passing to a gpu context that is on a separate channel.
  //   Note: a context in a separate process is necessarily on another channel.
  //   Use kVerifiedSyncToken.  Or use kUnverifiedSyncToken with a later call
  //   to VerifySyncTokensCHROMIUM()
  // Case 2: Passing to a gpu context that is on the same channel but not the
  //     same stream.
  //   Use kUnverifiedSyncToken
  // Case 3: Passing to a gpu context on the same stream.
  //   Use kOrderingBarrier
  virtual void EnsureMailbox(MailboxSyncMode) { NOTREACHED(); }
  virtual gpu::Mailbox GetMailbox() {
    NOTREACHED();
    return gpu::Mailbox();
  }
  virtual gpu::SyncToken GetSyncToken();
  virtual void UpdateSyncToken(gpu::SyncToken) { NOTREACHED(); }
  virtual bool IsPremultiplied() const { return true; }

  // Methods have exactly the same implementation for all sub-classes
  bool OriginClean() const { return is_origin_clean_; }
  void SetOriginClean(bool flag) { is_origin_clean_ = flag; }
  RefPtr<StaticBitmapImage> ConvertToColorSpace(sk_sp<SkColorSpace>,
                                                SkTransferFunctionBehavior);

 protected:
  // Helper for sub-classes
  void DrawHelper(PaintCanvas*,
                  const PaintFlags&,
                  const FloatRect&,
                  const FloatRect&,
                  ImageClampingMode,
                  const PaintImage&);

  // The following property is here because the SkImage API doesn't expose the
  // info. It is applied to both UnacceleratedStaticBitmapImage and
  // AcceleratedStaticBitmapImage. To change this property, the call site would
  // have to call SetOriginClean().
  bool is_origin_clean_ = true;
};

}  // namespace blink

#endif
