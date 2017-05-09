// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StaticBitmapImage_h
#define StaticBitmapImage_h

#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "platform/graphics/Image.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

class WebGraphicsContext3DProvider;

class PLATFORM_EXPORT StaticBitmapImage : public Image {
 public:
  static PassRefPtr<StaticBitmapImage> Create(sk_sp<SkImage>);

  // Methods overrided by all sub-classes
  virtual ~StaticBitmapImage() {}
  bool CurrentFrameKnownToBeOpaque(MetadataMode = kUseCurrentMetadata) = 0;
  sk_sp<SkImage> ImageForCurrentFrame() = 0;
  void Draw(PaintCanvas*,
            const PaintFlags&,
            const FloatRect& dst_rect,
            const FloatRect& src_rect,
            RespectImageOrientationEnum,
            ImageClampingMode) = 0;

  // Methods have common implementation for all sub-classes
  bool CurrentFrameIsComplete() override { return true; }
  void DestroyDecodedData() {}

  // Methods that have a default implementation, and overrided by only one
  // sub-class
  virtual bool HasMailbox() { return false; }
  virtual void Transfer() {}

  // Methods overrided by AcceleratedStaticBitmapImage only
  virtual void CopyToTexture(WebGraphicsContext3DProvider*,
                             GLenum,
                             GLuint,
                             bool,
                             const IntPoint&,
                             const IntRect&) {
    NOTREACHED();
  }
  virtual void EnsureMailbox() { NOTREACHED(); }
  virtual gpu::Mailbox GetMailbox() {
    NOTREACHED();
    return gpu::Mailbox();
  }
  virtual gpu::SyncToken GetSyncToken() {
    NOTREACHED();
    return gpu::SyncToken();
  }
  virtual void UpdateSyncToken(gpu::SyncToken) { NOTREACHED(); }

  // Methods have exactly the same implementation for all sub-classes
  bool OriginClean() const { return is_origin_clean_; }
  void SetOriginClean(bool flag) { is_origin_clean_ = flag; }
  bool IsPremultiplied() const { return is_premultiplied_; }
  void SetPremultiplied(bool flag) { is_premultiplied_ = flag; }

 protected:
  // Helper for sub-classes
  void DrawHelper(PaintCanvas*,
                  const PaintFlags&,
                  const FloatRect&,
                  const FloatRect&,
                  ImageClampingMode,
                  const PaintImage&);

  // These two properties are here because the SkImage API doesn't expose the
  // info. They applied to both UnacceleratedStaticBitmapImage and
  // AcceleratedStaticBitmapImage. To change these two properties, the call
  // site would have to call the API setOriginClean() and setPremultiplied().
  bool is_origin_clean_ = true;
  bool is_premultiplied_ = true;
};

}  // namespace blink

#endif
