// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_DRAW_UTILS_H_
#define PDF_DRAW_UTILS_H_

#include <stdint.h>

#include <vector>

#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/rect.h"

namespace chrome_pdf {

const uint8_t kOpaqueAlpha = 0xFF;
const uint8_t kTransparentAlpha = 0x00;

void AlphaBlend(const pp::ImageData& src,
                const pp::Rect& src_rc,
                pp::ImageData* dest,
                const pp::Point& dest_origin,
                uint8_t alpha_adjustment);

// Fill rectangle with gradient horizontally or vertically. Start is a color of
// top-left point of the rectangle, end color is a color of
// top-right (horizontal==true) or bottom-left (horizontal==false) point.
void GradientFill(pp::ImageData* image,
                  const pp::Rect& rc,
                  uint32_t start_color,
                  uint32_t end_color,
                  bool horizontal);

// Fill dirty rectangle with gradient, where gradient color set for corners of
// gradient rectangle. Parts of the dirty rect outside of gradient rect will
// be unchanged.
void GradientFill(pp::Instance* instance,
                  pp::ImageData* image,
                  const pp::Rect& dirty_rc,
                  const pp::Rect& gradient_rc,
                  uint32_t start_color,
                  uint32_t end_color,
                  bool horizontal,
                  uint8_t transparency);

// Copy one image into another. If stretch is true, the result occupy the entire
// dest_rc. If stretch is false, dest_rc.point will be used as an origin of the
// result image. Copy will ignore all pixels with transparent alpha from the
// source image.
void CopyImage(const pp::ImageData& src, const pp::Rect& src_rc,
               pp::ImageData* dest, const pp::Rect& dest_rc,
               bool stretch);

// Fill in rectangle with specified color.
void FillRect(pp::ImageData* image, const pp::Rect& rc, uint32_t color);

// Shadow Matrix contains matrix for shadow rendering. To reduce amount of
// calculations user may choose to cache matrix and reuse it if nothing changed.
class ShadowMatrix {
 public:
  // Matrix parameters.
  // depth - how big matrix should be. Shadow will go smoothly across the
  // entire matrix from black to background color.
  // If factor == 1, smoothing will be linear from 0 to the end (depth),
  // if 0 < factor < 1, smoothing will drop faster near 0.
  // if factor > 1, smoothing will drop faster near the end (depth).
  ShadowMatrix(uint32_t depth, double factor, uint32_t background);

  ~ShadowMatrix();

  uint32_t GetValue(int32_t x, int32_t y) const {
    return matrix_[y * depth_ + x];
  }

  uint32_t depth() const { return depth_; }
  double factor() const { return factor_; }
  uint32_t background() const { return background_; }

 private:
  uint32_t depth_;
  double factor_;
  uint32_t background_;
  std::vector<uint32_t> matrix_;
};

// Draw shadow on the image using provided ShadowMatrix.
// shadow_rc - rectangle occupied by shadow
// object_rc - rectangle that drops the shadow
// clip_rc - clipping region
void DrawShadow(pp::ImageData* image,
                const pp::Rect& shadow_rc,
                const pp::Rect& object_rc,
                const pp::Rect& clip_rc,
                const ShadowMatrix& matrix);

}  // namespace chrome_pdf

#endif  // PDF_DRAW_UTILS_H_
