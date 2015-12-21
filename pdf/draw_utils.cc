// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/draw_utils.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "base/numerics/safe_math.h"

namespace chrome_pdf {

inline uint8_t GetBlue(const uint32_t& pixel) {
  return static_cast<uint8_t>(pixel & 0xFF);
}

inline uint8_t GetGreen(const uint32_t& pixel) {
  return static_cast<uint8_t>((pixel >> 8) & 0xFF);
}

inline uint8_t GetRed(const uint32_t& pixel) {
  return static_cast<uint8_t>((pixel >> 16) & 0xFF);
}

inline uint8_t GetAlpha(const uint32_t& pixel) {
  return static_cast<uint8_t>((pixel >> 24) & 0xFF);
}

inline uint32_t MakePixel(uint8_t red,
                          uint8_t green,
                          uint8_t blue,
                          uint8_t alpha) {
  return (static_cast<uint32_t>(alpha) << 24) |
      (static_cast<uint32_t>(red) << 16) |
      (static_cast<uint32_t>(green) << 8) |
      static_cast<uint32_t>(blue);
}

inline uint8_t GradientChannel(uint8_t start, uint8_t end, double ratio) {
  double new_channel = start - (static_cast<double>(start) - end) * ratio;
  if (new_channel < 0)
    return 0;
  if (new_channel > 255)
    return 255;
  return static_cast<uint8_t>(new_channel + 0.5);
}

inline uint8_t ProcessColor(uint8_t src_color,
                            uint8_t dest_color,
                            uint8_t alpha) {
  uint32_t processed = static_cast<uint32_t>(src_color) * alpha +
                       static_cast<uint32_t>(dest_color) * (0xFF - alpha);
  return static_cast<uint8_t>((processed / 0xFF) & 0xFF);
}

inline bool ImageDataContainsRect(const pp::ImageData& image_data,
                                  const pp::Rect& rect) {
  return rect.width() >= 0 && rect.height() >= 0 &&
      pp::Rect(image_data.size()).Contains(rect);
}

void AlphaBlend(const pp::ImageData& src,
                const pp::Rect& src_rc,
                pp::ImageData* dest,
                const pp::Point& dest_origin,
                uint8_t alpha_adjustment) {
  if (src_rc.IsEmpty() || !ImageDataContainsRect(src, src_rc))
    return;

  pp::Rect dest_rc(dest_origin, src_rc.size());
  if (dest_rc.IsEmpty() || !ImageDataContainsRect(*dest, dest_rc))
    return;

  const uint32_t* src_origin_pixel = src.GetAddr32(src_rc.point());
  uint32_t* dest_origin_pixel = dest->GetAddr32(dest_origin);

  int height = src_rc.height();
  int width = src_rc.width();
  for (int y = 0; y < height; y++) {
    const uint32_t* src_pixel = src_origin_pixel;
    uint32_t* dest_pixel = dest_origin_pixel;
    for (int x = 0; x < width; x++) {
      uint8_t alpha =
          static_cast<uint8_t>(static_cast<uint32_t>(alpha_adjustment) *
                               GetAlpha(*src_pixel) / 0xFF);
      uint8_t red =
          ProcessColor(GetRed(*src_pixel), GetRed(*dest_pixel), alpha);
      uint8_t green =
          ProcessColor(GetGreen(*src_pixel), GetGreen(*dest_pixel), alpha);
      uint8_t blue =
          ProcessColor(GetBlue(*src_pixel), GetBlue(*dest_pixel), alpha);
      *dest_pixel = MakePixel(red, green, blue, GetAlpha(*dest_pixel));

      src_pixel++;
      dest_pixel++;
    }
    src_origin_pixel = reinterpret_cast<const uint32_t*>(
        reinterpret_cast<const char*>(src_origin_pixel) + src.stride());
    dest_origin_pixel = reinterpret_cast<uint32_t*>(
        reinterpret_cast<char*>(dest_origin_pixel) + dest->stride());
  }
}

void GradientFill(pp::ImageData* image,
                  const pp::Rect& rc,
                  uint32_t start_color,
                  uint32_t end_color,
                  bool horizontal) {
  std::vector<uint32_t> colors;
  colors.resize(horizontal ? rc.width() : rc.height());
  for (size_t i = 0; i < colors.size(); ++i) {
    double ratio = static_cast<double>(i) / colors.size();
    colors[i] = MakePixel(
        GradientChannel(GetRed(start_color), GetRed(end_color), ratio),
        GradientChannel(GetGreen(start_color), GetGreen(end_color), ratio),
        GradientChannel(GetBlue(start_color), GetBlue(end_color), ratio),
        GradientChannel(GetAlpha(start_color), GetAlpha(end_color), ratio));
  }

  if (horizontal) {
    const void* data = &(colors[0]);
    size_t size = colors.size() * 4;
    uint32_t* origin_pixel = image->GetAddr32(rc.point());
    for (int y = 0; y < rc.height(); y++) {
      memcpy(origin_pixel, data, size);
      origin_pixel = reinterpret_cast<uint32_t*>(
          reinterpret_cast<char*>(origin_pixel) + image->stride());
    }
  } else {
    uint32_t* origin_pixel = image->GetAddr32(rc.point());
    for (int y = 0; y < rc.height(); y++) {
      uint32_t* pixel = origin_pixel;
      for (int x = 0; x < rc.width(); x++) {
        *pixel = colors[y];
        pixel++;
      }
      origin_pixel = reinterpret_cast<uint32_t*>(
          reinterpret_cast<char*>(origin_pixel) + image->stride());
    }
  }
}

void GradientFill(pp::Instance* instance,
                  pp::ImageData* image,
                  const pp::Rect& dirty_rc,
                  const pp::Rect& gradient_rc,
                  uint32_t start_color,
                  uint32_t end_color,
                  bool horizontal,
                  uint8_t transparency) {
  pp::Rect draw_rc = gradient_rc.Intersect(dirty_rc);
  if (draw_rc.IsEmpty())
    return;

  pp::ImageData gradient(instance, PP_IMAGEDATAFORMAT_BGRA_PREMUL,
      gradient_rc.size(), false);

  GradientFill(&gradient, pp::Rect(pp::Point(), gradient_rc.size()),
               start_color, end_color, horizontal);

  pp::Rect copy_rc(draw_rc);
  copy_rc.Offset(-gradient_rc.x(), -gradient_rc.y());
  AlphaBlend(gradient, copy_rc, image, draw_rc.point(), transparency);
}

void CopyImage(const pp::ImageData& src, const pp::Rect& src_rc,
               pp::ImageData* dest, const pp::Rect& dest_rc,
               bool stretch) {
  if (src_rc.IsEmpty() || !ImageDataContainsRect(src, src_rc))
    return;

  pp::Rect stretched_rc(dest_rc.point(),
                        stretch ? dest_rc.size() : src_rc.size());
  if (stretched_rc.IsEmpty() || !ImageDataContainsRect(*dest, stretched_rc))
    return;

  const uint32_t* src_origin_pixel = src.GetAddr32(src_rc.point());
  uint32_t* dest_origin_pixel = dest->GetAddr32(dest_rc.point());
  if (stretch) {
    double x_ratio = static_cast<double>(src_rc.width()) / dest_rc.width();
    double y_ratio = static_cast<double>(src_rc.height()) / dest_rc.height();
    int32_t height = dest_rc.height();
    int32_t width = dest_rc.width();
    for (int32_t y = 0; y < height; ++y) {
      uint32_t* dest_pixel = dest_origin_pixel;
      for (int32_t x = 0; x < width; ++x) {
        uint32_t src_x = static_cast<uint32_t>(x * x_ratio);
        uint32_t src_y = static_cast<uint32_t>(y * y_ratio);
        const uint32_t* src_pixel = src.GetAddr32(
            pp::Point(src_rc.x() + src_x, src_rc.y() + src_y));
        *dest_pixel = *src_pixel;
        dest_pixel++;
      }
      dest_origin_pixel = reinterpret_cast<uint32_t*>(
          reinterpret_cast<char*>(dest_origin_pixel) + dest->stride());
    }
  } else {
    int32_t height = src_rc.height();
    base::CheckedNumeric<int32_t> width_bytes = src_rc.width();
    width_bytes *= 4;
    for (int32_t y = 0; y < height; ++y) {
      memcpy(dest_origin_pixel, src_origin_pixel, width_bytes.ValueOrDie());
      src_origin_pixel = reinterpret_cast<const uint32_t*>(
          reinterpret_cast<const char*>(src_origin_pixel) + src.stride());
      dest_origin_pixel = reinterpret_cast<uint32_t*>(
          reinterpret_cast<char*>(dest_origin_pixel) + dest->stride());
    }
  }
}

void FillRect(pp::ImageData* image, const pp::Rect& rc, uint32_t color) {
  int height = rc.height();
  if (height == 0)
    return;

  // Fill in first row.
  uint32_t* top_line = image->GetAddr32(rc.point());
  int width = rc.width();
  for (int x = 0; x < width; x++)
    top_line[x] = color;

  // Fill in the rest of the rectangle.
  int byte_width = width * 4;
  uint32_t* cur_line = reinterpret_cast<uint32_t*>(
          reinterpret_cast<char*>(top_line) + image->stride());
  for (int y = 1; y < height; y++) {
    memcpy(cur_line, top_line, byte_width);
    cur_line = reinterpret_cast<uint32_t*>(
            reinterpret_cast<char*>(cur_line) + image->stride());
  }
}

ShadowMatrix::ShadowMatrix(uint32_t depth, double factor, uint32_t background)
    : depth_(depth), factor_(factor), background_(background) {
  DCHECK(depth_ > 0);
  matrix_.resize(depth_ * depth_);

  // pv - is a rounding power factor for smoothing corners.
  // pv = 2.0 will make corners completely round.
  const double pv = 4.0;
  // pow_pv - cache to avoid recalculating pow(x, pv) every time.
  std::vector<double> pow_pv(depth_, 0.0);

  double r = static_cast<double>(depth_);
  double coef = 256.0 / pow(r, factor);

  for (uint32_t y = 0; y < depth_; y++) {
    // Since matrix is symmetrical, we can reduce the number of calculations
    // by mirroring results.
    for (uint32_t x = 0; x <= y; x++) {
      // Fill cache if needed.
      if (pow_pv[x] == 0.0)
        pow_pv[x] =  pow(x, pv);
      if (pow_pv[y] == 0.0)
        pow_pv[y] =  pow(y, pv);

      // v - is a value for the smoothing function.
      // If x == 0 simplify calculations.
      double v = (x == 0) ? y : pow(pow_pv[x] + pow_pv[y], 1 / pv);

      // Smoothing function.
      // If factor == 1, smoothing will be linear from 0 to the end,
      // if 0 < factor < 1, smoothing will drop faster near 0.
      // if factor > 1, smoothing will drop faster near the end (depth).
      double f = 256.0 - coef * pow(v, factor);

      uint8_t alpha = 0;
      if (f > kOpaqueAlpha)
        alpha = kOpaqueAlpha;
      else if (f < kTransparentAlpha)
        alpha = kTransparentAlpha;
      else
        alpha = static_cast<uint8_t>(f);

      uint8_t red = ProcessColor(0, GetRed(background), alpha);
      uint8_t green = ProcessColor(0, GetGreen(background), alpha);
      uint8_t blue = ProcessColor(0, GetBlue(background), alpha);
      uint32_t pixel = MakePixel(red, green, blue, GetAlpha(background));

      // Mirror matrix.
      matrix_[y * depth_ + x] = pixel;
      matrix_[x * depth_ + y] = pixel;
    }
  }
}

ShadowMatrix::~ShadowMatrix() {
}

void PaintShadow(pp::ImageData* image,
                 const pp::Rect& clip_rc,
                 const pp::Rect& shadow_rc,
                 const ShadowMatrix& matrix) {
  pp::Rect draw_rc = shadow_rc.Intersect(clip_rc);
  if (draw_rc.IsEmpty())
    return;

  int32_t depth = static_cast<int32_t>(matrix.depth());
  for (int32_t y = draw_rc.y(); y < draw_rc.bottom(); y++) {
    for (int32_t x = draw_rc.x(); x < draw_rc.right(); x++) {
      int32_t matrix_x = std::max(depth + shadow_rc.x() - x - 1,
                                  depth - shadow_rc.right() + x);
      int32_t matrix_y = std::max(depth + shadow_rc.y() - y - 1,
                                  depth - shadow_rc.bottom() + y);
      uint32_t* pixel = image->GetAddr32(pp::Point(x, y));

      if (matrix_x < 0)
        matrix_x = 0;
      else if (matrix_x >= static_cast<int32_t>(depth))
        matrix_x = depth - 1;

      if (matrix_y < 0)
        matrix_y = 0;
      else if (matrix_y >= static_cast<int32_t>(depth))
        matrix_y = depth - 1;

      *pixel = matrix.GetValue(matrix_x, matrix_y);
    }
  }
}

void DrawShadow(pp::ImageData* image,
                const pp::Rect& shadow_rc,
                const pp::Rect& object_rc,
                const pp::Rect& clip_rc,
                const ShadowMatrix& matrix) {
  if (shadow_rc == object_rc)
    return;  // Nothing to paint.

  // Fill top part.
  pp::Rect rc(shadow_rc.point(),
              pp::Size(shadow_rc.width(), object_rc.y() - shadow_rc.y()));
  PaintShadow(image, rc.Intersect(clip_rc), shadow_rc, matrix);

  // Fill bottom part.
  rc = pp::Rect(shadow_rc.x(), object_rc.bottom(),
                shadow_rc.width(), shadow_rc.bottom() - object_rc.bottom());
  PaintShadow(image, rc.Intersect(clip_rc), shadow_rc, matrix);

  // Fill left part.
  rc = pp::Rect(shadow_rc.x(), object_rc.y(),
                object_rc.x() - shadow_rc.x(), object_rc.height());
  PaintShadow(image, rc.Intersect(clip_rc), shadow_rc, matrix);

  // Fill right part.
  rc = pp::Rect(object_rc.right(), object_rc.y(),
                shadow_rc.right() - object_rc.right(), object_rc.height());
  PaintShadow(image, rc.Intersect(clip_rc), shadow_rc, matrix);
}

}  // namespace chrome_pdf

