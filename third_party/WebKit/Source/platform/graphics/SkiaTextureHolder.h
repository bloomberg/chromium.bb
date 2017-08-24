// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SkiaTextureHolder_h
#define SkiaTextureHolder_h

#include "platform/PlatformExport.h"
#include "platform/graphics/TextureHolder.h"

namespace blink {

class WebGraphicsContext3DProviderWrapper;

class PLATFORM_EXPORT SkiaTextureHolder final : public TextureHolder {
 public:
  ~SkiaTextureHolder() override;

  // Methods overriding TextureHolder
  bool IsSkiaTextureHolder() final { return true; }
  bool IsMailboxTextureHolder() final { return false; }
  IntSize Size() const final {
    return IntSize(image_->width(), image_->height());
  }
  bool IsValid() const final;
  bool CurrentFrameKnownToBeOpaque(Image::MetadataMode) final {
    return image_->isOpaque();
  }
  sk_sp<SkImage> GetSkImage() final { return image_; }
  void Abandon() final;

  // When creating a AcceleratedStaticBitmap from a texture-backed SkImage, this
  // function will be called to create a TextureHolder object.
  SkiaTextureHolder(sk_sp<SkImage>,
                    WeakPtr<WebGraphicsContext3DProviderWrapper>&&);
  // This function consumes the mailbox in the input parameter and turn it into
  // a texture-backed SkImage.
  SkiaTextureHolder(std::unique_ptr<TextureHolder>);

 private:
  //  void ReleaseImageThreadSafe();

  // The m_image should always be texture-backed
  sk_sp<SkImage> image_;
  THREAD_CHECKER(thread_checker_);
};

}  // namespace blink

#endif  // SkiaTextureHolder_h
