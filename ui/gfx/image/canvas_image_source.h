// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_IMAGE_CANVAS_IMAGE_SOURCE_H_
#define UI_GFX_IMAGE_CANVAS_IMAGE_SOURCE_H_

#include <utility>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_source.h"

namespace gfx {
class Canvas;
class ImageSkiaRep;

// CanvasImageSource is useful if you need to generate an image for
// a scale factor using gfx::Canvas. It creates a new Canvas
// with target scale factor and generates ImageSkiaRep when drawing is
// completed.
class GFX_EXPORT CanvasImageSource : public gfx::ImageSkiaSource {
 public:
  // Factory function to create an ImageSkia from a CanvasImageSource. Example:
  //   gfx::ImageSkia my_image =
  //       CanvasImageSource::MakeImageSkia<MySource>(param1, param2);
  template <typename T, typename... Args>
  static ImageSkia MakeImageSkia(Args&&... args) {
    auto source = std::make_unique<T>(std::forward<Args>(args)...);
    gfx::Size size = source->size();
    return gfx::ImageSkia(std::move(source), size);
  }

  CanvasImageSource(const gfx::Size& size, bool is_opaque);
  ~CanvasImageSource() override {}

  // Called when a new image needs to be drawn for a scale factor.
  virtual void Draw(gfx::Canvas* canvas) = 0;

  // Returns the size of images in DIP that this source will generate.
  const gfx::Size& size() const { return size_; };

  // Overridden from gfx::ImageSkiaSource.
  gfx::ImageSkiaRep GetImageForScale(float scale) override;

 protected:
  const gfx::Size size_;
  const bool is_opaque_;
  DISALLOW_COPY_AND_ASSIGN(CanvasImageSource);
};

}  // namespace gfx

#endif  // UI_GFX_IMAGE_CANVAS_IMAGE_SOURCE_H_
