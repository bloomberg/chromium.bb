/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// This file implements a small collection of filtered scanline based
// pixel operations, including scaling and shearing.

#include <emmintrin.h>
#include <pmmintrin.h>
#include <xmmintrin.h>

#include "native_client/tests/photo/fastmath.h"
#include "native_client/tests/photo/surface.h"

// Note: none of these functions are designed to be multi-thread safe.
// They must be used consistently from the same thread.

// Note: some functions utilize SSE and SSE2 intrinsics.  Currently, there
// are no generic C fallback implementations in this file for CPUs that do
// not support SSE and SSE2 instructions.  (Such support is left as an
// exercise.)  In the meantime, if a machine without SSE/SSE2 capabilities
// attempts to execute this code, an exception will be raised, terminating
// the Native Client application.

typedef float float4 __attribute__ ((vector_size (4*sizeof(float))));
typedef int32_t int4 __attribute__ ((vector_size (4*sizeof(int32_t))));

static const int4 zero = { 0, 0, 0, 0 };

// use SSE intrinsics to expand a uint32_t into a 4 element float vector
INLINE_NO_INSTRUMENT __m128 UnpackRGBA(uint32_t rgba);
inline __m128 UnpackRGBA(uint32_t rgba) {
  // expand 4 unsigned bytes to 4 32-bit zero extended integers
  __m128i x = _mm_cvtsi32_si128(rgba);
  __m128i y = _mm_unpacklo_epi8(x, zero);
  __m128i z = _mm_unpacklo_epi16(y, zero);
  // convert to float vector
  __m128 f = _mm_cvtepi32_ps(z);
  return f;
}


// use SSE intrinsics to pack a 4 element float vector downto a uint32_t
// The result will be saturated (clamped)
INLINE_NO_INSTRUMENT uint32_t PackRGBA(float4 clr);
inline uint32_t PackRGBA(float4 clr) {
  // squish 4 floats into 4 8-bit unsigned chars
  // saturate (clamp) 0..1 (float) to 0..255 (uchar)
  __m128i a = _mm_cvttps_epi32(clr);
  __m128i b = _mm_packs_epi32(a, zero);
  __m128i c = _mm_packus_epi16(b, zero);
  int r = _mm_cvtsi128_si32(c);
  return r;
}


// fill a horizontal scanline with a color, assume inputs are already
// clipped to fit within the surface.
void Surface::FillScanlineNoClip(int x0, int x1, int y, uint32_t color) {
  int yoffset = y * width_;
  for (int i = x0; i < x1; ++i)
    pixels_[yoffset + i] = color;
}


// fill a horizontal scanline with a color, perform clipping first
void Surface::FillScanline(int x0, int x1, int y, uint32_t color) {
  int yoffset;
  // do clipping
  if (y < 0) return;
  if (y >= height_) return;
  if (x0 >= width_) return;
  if (x1 < 0) return;
  yoffset = y * width_;
  if (x0 < 0) x0 = 0;
  if (x1 > width_) x1 = width_;
  // fill the region
  for (int i = x0; i < x1; ++i)
    pixels_[yoffset + i] = color;
}


// fill a vertical scanline with a color, perform clipping first
void Surface::FillScanlineV(int x, int y0, int y1, uint32_t color) {
  int yoffset;
  // do clipping
  if (x < 0) return;
  if (x >= width_) return;
  if (y0 >= height_) return;
  if (y1 < 0) return;
  yoffset = y0 * width_ + x;
  if (y0 < 0) y0 = 0;
  if (y1 > height_) y1 = height_;
  // fill the region
  for (int i = y0; i < y1; ++i)
    pixels_[yoffset] = color;
    yoffset += this->width_;
}


// fill a rectangular region with a solid color, perform clipping first
void Surface::FillRect(int x0, int y0, int x1, int y1, uint32_t color) {
  // origin is upper left
  // x0, y0 is upper left and x1, y1 is lower right
  // do clipping
  if (x0 < 0) x0 = 0;
  if (x0 > width_) return;
  if (y0 < 0) y0 = 0;
  if (y0 > height_) return;
  if (x1 < 0) return;
  if (x1 > width_) x1 = width_;
  if (y1 < 0) return;
  if (y1 > height_) y1 = height_;
  // fill without clipping
  for (int i = y0; i < y1; ++i)
    FillScanlineNoClip(x0, x1, i, color);
}


// clear the entire surface with a solid color
void Surface::Clear(uint32_t color) {
  FillRect(0, 0, width_, height_, color);
}


// blit (copy) a horizontal scanline from one location to another
// performs clipping
void Surface::BlitScanline(int dx, int dy, int sx0, int sx1, int sy,
                           Surface *src) {
  uint32_t *psrc;
  uint32_t *pdst;

  if (dy < 0) return;
  if (dy >= height_) return;
  if (dx >= width_) return;
  if ((dx + (sx1 - sx0)) < 0) return;

  if (dx < 0) {
    sx0 -= dx;
    dx = 0;
  }

  if ((dx + (sx1 - sx0)) > width_)
    sx1 = sx0 + (width_ - dx);

  psrc = &src->pixels_[sy * src->width_ + sx0];
  pdst = &this->pixels_[dy * this->width_ + dx];

  // if fetching from outside of src, fill dest with borderColor
  while (sx0 < 0) {
    *pdst++ = src->borderColor_;
    ++sx0;
    ++dx;
  }

  int sx1_copy = sx1 <= src->width_ ? sx1 : src->width_;
  int i = sx0;

  while (i < sx1_copy) {
    *pdst++ = *psrc++;
    ++i;
  }

  while (i < sx1) {
    *pdst++ = src->borderColor_;
    ++i;
  }
}


// Blit (copy) a horizontal scanline from one location to another.
// Assumes inputs have already been clipped.
void Surface::BlitScanlineNoClip(int dx, int dy, int sx0, int sx1, int sy,
                                 Surface *src) {
  uint32_t *psrc;
  uint32_t *pdst;
  psrc = &src->pixels_[sy * src->width_ + sx0];
  pdst = &this->pixels_[dy * this->width_ + dx];
  for (int i = sx0; i < sx1; ++i)
    *pdst++ = *psrc++;
}


// Realloc a surface to a new size.  If the inplace flag is set,
// try to recycle existing memory if the new size fits.  Otherwise, free
// the old pixel array and allocate a new one.
void Surface::Realloc(int w, int h, bool inplace) {
  int newsize = w * h;
  width_ = w;
  height_ = h;
  if ((inplace) && (newsize <= size_))
    return;
  // replace the pixel array
  delete[] pixels_;
  size_ = newsize;
  pixels_ = new uint32_t[size_];
}


// copy an entire source surface into the destination surface, at
// destination location x, y.
void Surface::Blit(int x, int y, Surface *src) {
  // blit entire src pixel map into dest at x, y
  int sx0 = 0;
  int sx1 = src->width_;
  for (int j = 0; j < src->height_; j++) {
    BlitScanline(x, y + j, sx0, sx1, j, src);
  }
}


// only scales down in size...
void Surface::ScaledBlitScanline(float dx0, float dx1, int dy,
    float sx0, float sx1, int sy, Surface *src, int filter) {
  if (dy < 0) return;
  if (dy >= height_) return;
  if ((sy < 0) || (sy >= src->height_)) {
    FillScanline(static_cast<int>(dx0), static_cast<int>(dx1),
        dy, src->borderColor_);
    return;
  }
  float delta_sx = sx1 - sx0;
  float delta_dx = dx1 - dx0;
  float delta_dx_o_delta_sx =  delta_dx / delta_sx;
  float oofilter = 1.0f / filter;
  float4 vc = {0.0f, 0.0f, 0.0f, 0.0f};
  const float4 vone = {1.0f, 1.0f, 1.0f, 1.0f};
  const float4 vzero = {0.0f, 0.0f, 0.0f, 0.0f};
  float4 vborderColor;
  float v = 0.0f;
  int lastiu = 0;
  float4 vdivby = {0.0f, 0.0f, 0.0f, 0.0f};
  int leftmostu = 0;
  int rightmostu = this->width_;
  int leftmosts = 0;
  int rightmosts = src->width_;
  uint32_t *psrc = &src->pixels_[sy * src->width_];
  uint32_t *pdst = &this->pixels_[dy * this->width_];
  vborderColor = UnpackRGBA(src->borderColor_);
  while (v < delta_sx) {
    float u = dx0 + v * delta_dx_o_delta_sx;
    int iu = static_cast<int>(u);
    if (iu != lastiu) {
      if ((lastiu >= leftmostu) && (lastiu < rightmostu)) {
        float4 voodivby = _mm_rcp_ss(vdivby);
        float4 voodivbys = _mm_shuffle_ps(voodivby, voodivby, 0);
        float4 vnc = _mm_mul_ps(vc, voodivbys);
        uint32_t c = PackRGBA(vnc);
        pdst[lastiu] = c;
      }
      vc = vzero;
      vdivby = vzero;
      lastiu = iu;
    }
    float4 vsc;
    int s = static_cast<int>(sx0 + v);
    if ((s >= leftmosts) && (s < rightmosts)) {
      uint32_t sc = psrc[s];
      vsc = UnpackRGBA(sc);
    } else {
      vsc = vborderColor;
    }
    vc += vsc;
    vdivby = _mm_add_ss(vdivby, vone);
    v += oofilter;
  }
  // put remaining bit down
  if ((lastiu >= leftmostu) && (lastiu < rightmostu)) {
    float4 voodivby = _mm_rcp_ss(vdivby);
    float4 voodivbys = _mm_shuffle_ps(voodivby, voodivby, 0);
    float4 vnc = _mm_mul_ps(vc, voodivbys);
    uint32_t c = PackRGBA(vnc);
    pdst[lastiu] = c;
  }
}


void Surface::ScaledBlitScanlineV(int dx, float dy0, float dy1,
    int sx, float sy0, float sy1, Surface *src, int filter) {
  if (dx < 0) return;
  if (dx >= width_) return;
  if ((sx < 0) || (sx >= src->width_)) {
    FillScanlineV(dx, static_cast<int>(dy0), static_cast<int>(dy1),
        src->borderColor_);
    return;
  }
  float delta_sy = sy1 - sy0;
  float delta_dy = dy1 - dy0;
  float delta_dy_o_delta_sy =  delta_dy / delta_sy;
  float oofilter = 1.0f / filter;
  float4 vc = {0.0f, 0.0f, 0.0f, 0.0f};
  const float4 vone = {1.0f, 1.0f, 1.0f, 1.0f};
  const float4 vzero = {0.0f, 0.0f, 0.0f, 0.0f};
  float4 vborderColor;
  float v = 0.0f;
  int lastiu = 0;
  float4 vdivby = {0.0f, 0.0f, 0.0f, 0.0f};
  int topmostu = 0;
  int bottommostu = this->height_;
  int topmosts = 0;
  int bottommosts = src->height_;
  uint32_t *psrc = &src->pixels_[sx];
  uint32_t *pdst = &this->pixels_[dx];
  vborderColor = UnpackRGBA(src->borderColor_);
  while (v < delta_sy) {
    float u = dy0 + v * delta_dy_o_delta_sy;
    int iu = static_cast<int>(u);
    if (iu != lastiu) {
      if ((lastiu >= topmostu) && (lastiu < bottommostu)) {
        float4 voodivby = _mm_rcp_ss(vdivby);
        float4 voodivbys = _mm_shuffle_ps(voodivby, voodivby, 0);
        float4 vnc = _mm_mul_ps(vc, voodivbys);
        uint32_t c = PackRGBA(vnc);
        pdst[lastiu * this->width_] = c;
      }
      vc = vzero;
      vdivby = vzero;
      lastiu = iu;
    }
    float4 vsc;
    int s = static_cast<int>(sy0 + v);
    if ((s >= topmosts) && (s < bottommosts)) {
      uint32_t sc = psrc[s * src->width_];
      vsc = UnpackRGBA(sc);
    } else {
      vsc = vborderColor;
    }
    vc += vsc;
    vdivby = _mm_add_ss(vdivby, vone);
    v += oofilter;
  }
  // put remaining bit down
  if ((lastiu >= topmostu) && (lastiu < bottommostu)) {
    float4 voodivby = _mm_rcp_ss(vdivby);
    float4 voodivbys = _mm_shuffle_ps(voodivby, voodivby, 0);
    float4 vnc = _mm_mul_ps(vc, voodivbys);
    uint32_t c = PackRGBA(vnc);
    pdst[lastiu * this->width_] = c;
  }
}


void Surface::RescaleFrom(Surface *src, float sx, float sy, Surface *tmp) {
  tmp->Realloc(src->width_ * sx, src->height_, true);
  for (int i = 0; i < src->height_; ++i) {
    tmp->ScaledBlitScanline(0.0f, src->width_ * sx, i, 0.0f,
        static_cast<float>(src->width_), i, src, 2);
  }
  this->Realloc(src->width_ * sx, src->height_ * sy, true);
  for (int i = 0; i < tmp->width_; ++i) {
    this->ScaledBlitScanlineV(i, 0.0f, tmp->height_ * sy, i, 0.0f,
        static_cast<float>(tmp->height_), tmp, 2);
  }
}


void Surface::ShearedBlitScanline(float dtx0, float dbx0, int dy,
    int sx0, int sx1, int sy, Surface *src) {
  // only limited angle shearing is supported
  //  - up to three source pixels can contribute to a destination pixel
  //  - shearing only ... no scaling, no subpixel source offset
  float dtx0c = floor(dtx0 + 1.0f);
  float dbx0c = floor(dbx0 + 1.0f);
  uint32_t src_border_color = src->borderColor_;

  // are we writing entirely out of bounds?
  if ((dy < 0) || (dy >= this->height_)) {
    return;
  }
  if (static_cast<int>(dtx0) > this->width_) {
    return;
  }
  if ((static_cast<int>(dtx0) + (sx1 - sx0)) < 0) {
    return;
  }
  // are we reading entirely outside of source?
  if ((sx0 >= src->width_) || (sx1 < 0) || (sy < 0) || (sy >= src->height_)) {
    // Note: implement anti-aliased fill and return... (endpoints need to be AA)
    // In the meantime, fall through to blit code below, which can still
    // handle this situation -- it will just be a little slower.
  }
  float f0;
  float f1;
  float f2;
  if (static_cast<int>(dtx0c) == static_cast<int>(dbx0c)) {
    // both leftmost corners are in the same dest pixel
    // and the filter only applies to two source pixels
    f2 = 0.5f * (dtx0 - dbx0) + (dtx0c - dtx0);
    f1 = 1.0f - f2;
    f0 = 0.0f;
  } else {
    // each leftmost corner falls into different dest pixels
    // and the filter applies to three source pixels
    if (dbx0 < dtx0) {
      f2 = 0.5f * (dbx0c - dbx0);
      f0 = 0.5f * (dtx0 - dbx0c);
    } else {
      f2 = 0.5f * (dtx0c - dtx0);
      f0 = 0.5f * (dbx0 - dtx0c);
    }
    f1 = 1.0f - (f0 + f2);
  }

  // build temp vectors
  float4 vf0 = {f0, f0, f0, 1.0f};
  float4 vf1 = {f1, f1, f1, 0.0f};
  float4 vf2 = {f2, f2, f2, 0.0f};
  float4 vborderColor = UnpackRGBA(src_border_color);
  float4 vc0 = vborderColor;
  float4 vc1 = vborderColor;
  float4 vc2 = vborderColor;

  int sx = sx0;
  int dx = (dtx0 < dbx0) ?
      static_cast<int>(floor(dtx0)) :
      static_cast<int>(floor(dbx0));
  uint32_t *srcp = &src->pixels_[sy * src->width_];
  uint32_t *dstp = &this->pixels_[dy * this->width_];

  // skip pixels off the left side
  // jump to left edge, but offset by -2 so we can prime the pump
  if (dx < 0) {
    sx = sx - dx - 2;
    dx = -2;

    // do two pixels worth to prime
    for (int i = 0; i < 2; ++i) {
      // rotate color registers
      vc0 = vc1;
      vc1 = vc2;
      if ((sx >= 0) && (sx < src->width_)) {
        uint32_t sc = srcp[sx];
        vc2 = UnpackRGBA(sc);
      } else {
        vc2 = vborderColor;
      }
      ++dx;
      ++sx;
    }
  }

  // compute stopping point, clip against right edge if needed
  int end_dx = dx + (sx1 - sx);
  if (end_dx > this->width_) {
    end_dx = this->width_;
  }

  while (dx < end_dx) {
    // rotate color registers
    vc0 = vc1;
    vc1 = vc2;

    // read in the next color
    if ((sx >= 0) && (sx < src->width_)) {
      uint32_t sc = srcp[sx];
      vc2 = UnpackRGBA(sc);
    } else {
      vc2 = vborderColor;
    }

    // compute filtered color value (example)
    __m128 vtmp0 = _mm_mul_ps(vc0, vf0);
    __m128 vtmp1 = _mm_mul_ps(vc1, vf1);
    __m128 vtmp2 = _mm_mul_ps(vc2, vf2);
    __m128 vacc0 = _mm_add_ps(vtmp0, vtmp1);
    __m128 vclr = _mm_add_ps(vacc0, vtmp2);

    // store filtered color value
    dstp[dx] = PackRGBA(vclr);

    // next pixel
    ++dx;
    ++sx;
  }
}


void Surface::ShearedBlitScanlineV(int dx, float dly0, float dry0,
    int sx, int sy0, int sy1, Surface *src) {
  // only limited angle shearing is supported
  //  - up to three source pixels can contribute to a destination pixel
  //  - shearing only ... no scaling, no subpixel source offset
  float dly0c = floor(dly0 + 1.0f);
  float dry0c = floor(dry0 + 1.0f);
  uint32_t src_border_color = src->borderColor_;

  // are we writing entirely out of bounds?
  if ((dx < 0) || (dx >= this->width_)) {
    return;
  }
  if (static_cast<int>(dly0) > this->height_) {
    return;
  }
  if ((static_cast<int>(dly0) + (sy1 - sy0)) < 0) {
    return;
  }
  // are we reading entirely outside of source?
  if ((sy0 >= src->height_) || (sy1 < 0) || (sx < 0) || (sx >= src->width_)) {
    // Note: implement anti-aliased fill and return... (endpoints need to be AA)
    // In the meantime, fall through to blit code below, which can still
    // handle this situation -- it will just be a little slower.
  }
  float f0;
  float f1;
  float f2;
  if (static_cast<int>(dly0c) == static_cast<int>(dry0c)) {
    // both topmost corners are in the same dest pixel
    // and the filter only applies to two source pixels
    f2 = 0.5f * (dly0 - dry0) + (dly0c - dly0);
    f1 = 1.0f - f2;
    f0 = 0.0f;
  } else {
    // each topmost corner falls into different dest pixels
    // and the filter applies to three source pixels
    if (dry0 < dly0) {
      f2 = 0.5f * (dry0c - dry0);
      f0 = 0.5f * (dly0 - dry0c);
    } else {
      f2 = 0.5f * (dly0c - dly0);
      f0 = 0.5f * (dry0 - dly0c);
    }
    f1 = 1.0f - (f0 + f2);
  }

  // build temp vectors
  float4 vf0 = {f0, f0, f0, 1.0f};
  float4 vf1 = {f1, f1, f1, 0.0f};
  float4 vf2 = {f2, f2, f2, 0.0f};
  float4 vborderColor = UnpackRGBA(src_border_color);
  float4 vc0 = vborderColor;
  float4 vc1 = vborderColor;
  float4 vc2 = vborderColor;

  int sy = sy0;
  int dy = (dly0 < dry0) ?
      static_cast<int>(floor(dly0)) :
      static_cast<int>(floor(dry0));
  uint32_t *srcp = &src->pixels_[sx];
  uint32_t *dstp = &this->pixels_[dx];

  // skip pixels above the top
  if (dy < 0) {
    // jump to top edge, but offset by -2 so we can prime the pump
    sy = sy - dy - 2;
    dy = -2;

    // do two pixels worth to prime
    for (int i = 0; i < 2; ++i) {
      vc0 = vc1;
      vc1 = vc2;
      if ((sy >= 0) && (sy < src->height_)) {
        // within source, use read from source pixel
        uint32_t sc = srcp[sy * src->width_];
        vc2 = UnpackRGBA(sc);
      } else {
        // otherwise use src's border color
        vc2 = vborderColor;
      }
      ++dy;
      ++sy;
    }
  }

  // compute stopping point, clip to bottom edge if needed
  int end_dy = dy + (sy1 - sy);
  if (end_dy > this->height_) {
    end_dy = this->height_;
  }

  while (dy < end_dy) {
    vc0 = vc1;
    vc1 = vc2;
    if ((sy >= 0) && (sy < src->height_)) {
      // within source, so read from source pixel
      uint32_t sc = srcp[sy * src->width_];
      vc2 = UnpackRGBA(sc);
    } else {
      // otherwise use src's border color
      vc2 = vborderColor;
    }

    // compute filtered color value
    __m128 vtmp0 = _mm_mul_ps(vc0, vf0);
    __m128 vtmp1 = _mm_mul_ps(vc1, vf1);
    __m128 vtmp2 = _mm_mul_ps(vc2, vf2);
    __m128 vacc0 = _mm_add_ps(vtmp0, vtmp1);
    __m128 vclr = _mm_add_ps(vacc0, vtmp2);

    // store filtered color value
    dstp[dy * this->width_] = PackRGBA(vclr);

    ++dy;
    ++sy;
  }
}


// rotate a bitmap via 3 shearing operations
// slower than single pass, but easier to implement with good filtering
// Note: at some point, implement cropping on the result, such that the
// largest axis-aligned rectangle which fits within the rotated rectangle.
void Surface::Rotate(float degrees, Surface *src, Surface *tmp) {
  float org_x = static_cast<float>(src->width_) * 0.5f;
  float org_y = static_cast<float>(src->height_) * 0.5f;
  float radians = M_PI * degrees / 180.0f;
  float tanarg = -tan(radians * 0.5f);
  float sinarg = sin(radians);

  tmp->Realloc(src->width_, src->height_, true);
  tmp->Clear();

  // rotation -45..45 degrees as a product of 3 shears

  // 1) horizontal shear
  int iy = 0;
  for (float y = -org_y; y < org_y; y += 1.0f) {
    float ty = y;
    float by = y + 1.0f;
    float tx = -org_x + tanarg * ty;
    float bx = -org_x + tanarg * by;
    ShearedBlitScanline(tx + org_x, bx + org_x, iy, 0, src->width_, iy, src);
    iy = iy + 1;
  }

  // 2) vertical shear
  int ix = 0;
  for (float x = -org_x; x < org_x; x += 1.0f) {
    float lx = x;
    float rx = x + 1.0f;
    float ly = -org_y + sinarg * lx;
    float ry = -org_y + sinarg * rx;
    tmp->ShearedBlitScanlineV(ix, ly + org_y, ry + org_y, ix, 0,
        this->height_, this);
    ix = ix + 1;
  }

  // 3) horizontal shear
  iy = 0;
  for (float y = -org_y; y < org_y; y += 1.0f) {
    float ty = y;
    float by = y + 1.0f;
    float tx = -org_x + tanarg * ty;
    float bx = -org_x + tanarg * by;
    ShearedBlitScanline(tx + org_x, bx + org_x, iy, 0, tmp->width_, iy, tmp);
    iy = iy + 1;
  }
}


Surface::Surface(int w, int h, uint32_t borderColor) {
  width_ = w;
  height_ = h;
  borderColor_ = borderColor;
  size_ = width_ * height_;
  pixels_ = new uint32_t[size_];
  Clear();
}


Surface::~Surface() {
  delete[] pixels_;
}
