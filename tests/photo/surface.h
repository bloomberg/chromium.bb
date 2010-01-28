/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define INLINE_NO_INSTRUMENT \
    __attribute__((no_instrument_function, always_inline))


#define MAX_RECIPROCAL_TABLE 256

// build a packed color
INLINE_NO_INSTRUMENT
    uint32_t MakeRGBA(uint32_t r, uint32_t g, uint32_t b, uint32_t a);
inline uint32_t MakeRGBA(uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
  return (((a) << 24) | ((r) << 16) | ((g) << 8) | (b));
}

// extract R, G, B, A from packed color
INLINE_NO_INSTRUMENT int ExtractR(uint32_t c);
inline int ExtractR(uint32_t c) {
  return (c >> 16) & 0xFF;
}

INLINE_NO_INSTRUMENT int ExtractG(uint32_t c);
inline int ExtractG(uint32_t c) {
  return (c >> 8) & 0xFF;
}

INLINE_NO_INSTRUMENT int ExtractB(uint32_t c);
inline int ExtractB(uint32_t c) {
  return c & 0xFF;
}

INLINE_NO_INSTRUMENT int ExtractA(uint32_t c);
inline int ExtractA(uint32_t c) {
  return (c >> 24) & 0xFF;
}

class Surface {
 public:
  int width_, height_;
  int size_;
  uint32_t borderColor_;
  uint32_t *pixels_;
  void SetBorderColor(uint32_t color) { borderColor_ = color; }
  uint32_t GetBorderColor() { return borderColor_; }
  void PutPixelNoClip(int x, int y, uint32_t color) {
    pixels_[y * width_ + x] = color;
  }
  uint32_t GetPixel(int x, int y) {
    if ((x < 0) || (x >= width_) || (y < 0) || (y >= height_)) {
      return borderColor_;
    }
    return pixels_[y * width_ + x];
  }
  uint32_t GetPixelNoClip(int x, int y) {
    return pixels_[y * width_ + x];
  }
  void PutPixel(int x, int y, uint32_t color) {
    if ((x >= 0) && (x < width_) && (y >= 0) && (y < height_)) {
      pixels_[y * width_ + x] = color;
    }
  }

  void FillScanlineNoClip(int x0, int x1, int y, uint32_t color);
  void FillScanline(int x0, int x1, int y, uint32_t color);
  void FillScanlineV(int x, int y0, int y1, uint32_t color);
  void FillRect(int x0, int y0, int x1, int y1, uint32_t color);
  void Clear(uint32_t color);
  void Clear() { Clear(this->borderColor_); }
  void BlitScanline(int dx, int dy, int sx0, int sx1, int sy,
                    Surface *src);
  void BlitScanlineNoClip(int dx, int dy, int sx0, int sx1, int sy,
                    Surface *src);
  void Blit(int dx, int dy, Surface *src);

  void Realloc(int w, int h, bool inplace);
  void ScaledBlitScanline(float dx0, float dx1, int dy,
    float sx0, float sx1, int sy, Surface *src, int filter);
  void ScaledBlitScanlineV(int dx, float dy0, float dy1,
    int sx, float sy0, float sy1, Surface *src, int filter);
  void ShearedBlitScanline(float dtx0, float dbx0, int dy,
    int sx0, int sx1, int sy, Surface *src);
  void ShearedBlitScanlineV(int dx, float dly0, float dry0,
    int sx, int sy0, int sy1, Surface *src);

  void Rotate(float degrees, Surface *src, Surface *tmp);

  void RescaleFrom(Surface *src, float sx, float sy, Surface *tmp);

  Surface(int w, int h, uint32_t borderColor = MakeRGBA(0, 255, 0, 255));
  ~Surface();
};
