// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_IMAGE_PROVIDER_H_
#define CC_PAINT_IMAGE_PROVIDER_H_

#include "base/callback.h"
#include "base/optional.h"
#include "cc/paint/decoded_draw_image.h"
#include "cc/paint/draw_image.h"
#include "cc/paint/paint_export.h"

#include <vector>

namespace cc {
class PaintImage;

// Used to replace lazy generated PaintImages with decoded images for
// rasterization.
class CC_PAINT_EXPORT ImageProvider {
 public:
  class CC_PAINT_EXPORT ScopedResult {
   public:
    using DestructionCallback = base::OnceClosure;

    ScopedResult();
    explicit ScopedResult(DecodedDrawImage image);
    ScopedResult(DecodedDrawImage image, DestructionCallback callback);
    ScopedResult(const PaintRecord* record, DestructionCallback callback);
    ~ScopedResult();

    ScopedResult(ScopedResult&& other);
    ScopedResult& operator=(ScopedResult&& other);

    operator bool() const { return image_ || record_; }
    const DecodedDrawImage& decoded_image() const { return image_; }
    bool needs_unlock() const { return !destruction_callback_.is_null(); }
    const PaintRecord* paint_record() {
      DCHECK(record_);
      return record_;
    }

   private:
    void DestroyDecode();

    DecodedDrawImage image_;
    const PaintRecord* record_ = nullptr;
    DestructionCallback destruction_callback_;

    DISALLOW_COPY_AND_ASSIGN(ScopedResult);
  };

  virtual ~ImageProvider() {}

  // Returns either:
  // 1. The DecodedDrawImage to use for this PaintImage. If no image is
  // provided, the draw for this image will be skipped during raster. Or,
  // 2. The PaintRecord produced by paint worklet JS paint callback.
  virtual ScopedResult GetRasterContent(const DrawImage& draw_image) = 0;
};

}  // namespace cc

#endif  // CC_PAINT_IMAGE_PROVIDER_H_
