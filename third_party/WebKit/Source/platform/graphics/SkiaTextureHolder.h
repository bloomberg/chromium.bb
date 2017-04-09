// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SkiaTextureHolder_h
#define SkiaTextureHolder_h

#include "platform/PlatformExport.h"
#include "platform/graphics/TextureHolder.h"

namespace blink {

class PLATFORM_EXPORT SkiaTextureHolder final : public TextureHolder {
 public:
  ~SkiaTextureHolder() override;

  // Methods overriding TextureHolder
  bool IsSkiaTextureHolder() final { return true; }
  bool IsMailboxTextureHolder() final { return false; }
  unsigned SharedContextId() final;
  IntSize size() const final {
    return IntSize(image_->width(), image_->height());
  }
  bool CurrentFrameKnownToBeOpaque(Image::MetadataMode) final {
    return image_->isOpaque();
  }
  sk_sp<SkImage> GetSkImage() final { return image_; }
  void SetSharedContextId(unsigned context_id) final {
    shared_context_id_ = context_id;
  }

  // When creating a AcceleratedStaticBitmap from a texture-backed SkImage, this
  // function will be called to create a TextureHolder object.
  SkiaTextureHolder(sk_sp<SkImage>);
  // This function consumes the mailbox in the input parameter and turn it into
  // a texture-backed SkImage.
  SkiaTextureHolder(std::unique_ptr<TextureHolder>);

 private:
  void ReleaseImageThreadSafe();

  // The m_image should always be texture-backed
  sk_sp<SkImage> image_;
  // Id of the shared context where m_image was created
  unsigned shared_context_id_ = 0;
};

}  // namespace blink

#endif  // SkiaTextureHolder_h
