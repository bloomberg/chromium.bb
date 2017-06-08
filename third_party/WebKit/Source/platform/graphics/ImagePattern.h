// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImagePattern_h
#define ImagePattern_h

#include "platform/graphics/Pattern.h"

class SkImage;

namespace blink {

class Image;

class PLATFORM_EXPORT ImagePattern final : public Pattern {
 public:
  static PassRefPtr<ImagePattern> Create(PassRefPtr<Image>, RepeatMode);

  bool IsTextureBacked() const override;

 protected:
  sk_sp<PaintShader> CreateShader(const SkMatrix&) override;
  bool IsLocalMatrixChanged(const SkMatrix&) const override;

 private:
  ImagePattern(PassRefPtr<Image>, RepeatMode);
  SkMatrix previous_local_matrix_;

  sk_sp<SkImage> tile_image_;
};

}  // namespace blink

#endif /* ImagePattern_h */
