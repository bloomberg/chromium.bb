
/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contains the functions to help with images.

// The precompiled header must appear before anything else.
#include "core/cross/precompile.h"

#include "core/cross/image_utils.h"
#include "core/cross/pointer_utils.h"
#include "core/cross/math_utilities.h"

namespace o3d {
namespace image {

// Computes the size of the buffer containing a an image, given its width,
// height and format.
size_t ComputeBufferSize(unsigned int width,
                         unsigned int height,
                         Texture::Format format) {
  DCHECK(CheckImageDimensions(width, height));
  unsigned int pixels = width * height;
  switch (format) {
    case Texture::XRGB8:
    case Texture::ARGB8:
      return 4 * sizeof(uint8) * pixels;  // NOLINT
    case Texture::ABGR16F:
      return 4 * sizeof(uint16) * pixels;  // NOLINT
    case Texture::R32F:
      return sizeof(float) * pixels;  // NOLINT
    case Texture::ABGR32F:
      return 4 * sizeof(float) * pixels;  // NOLINT
    case Texture::DXT1:
    case Texture::DXT3:
    case Texture::DXT5: {
      unsigned int blocks = ((width + 3) / 4) * ((height + 3) / 4);
      unsigned int bytes_per_block = format == Texture::DXT1 ? 8 : 16;
      return blocks * bytes_per_block;
    }
    case Texture::UNKNOWN_FORMAT:
      break;
  }
  // failed to find a matching format
  LOG(ERROR) << "Unrecognized Texture format type.";
  return 0;
}

// Gets the size of the buffer containing a mip-map chain, given its base
// width, height, format and number of mip-map levels.
size_t ComputeMipChainSize(unsigned int base_width,
                           unsigned int base_height,
                           Texture::Format format,
                           unsigned int num_mipmaps) {
  DCHECK(CheckImageDimensions(base_width, base_height));
  size_t total_size = 0;
  unsigned int mip_width = base_width;
  unsigned int mip_height = base_height;
  for (unsigned int i = 0; i < num_mipmaps; ++i) {
    total_size += ComputeBufferSize(mip_width, mip_height, format);
    mip_width = std::max(1U, mip_width >> 1);
    mip_height = std::max(1U, mip_height >> 1);
  }
  return total_size;
}

// Scales the image using basic point filtering.
bool ScaleUpToPOT(unsigned int width,
                  unsigned int height,
                  Texture::Format format,
                  const uint8 *src,
                  uint8 *dst,
                  int dst_pitch) {
  DCHECK(CheckImageDimensions(width, height));
  switch (format) {
    case Texture::XRGB8:
    case Texture::ARGB8:
    case Texture::ABGR16F:
    case Texture::R32F:
    case Texture::ABGR32F:
      break;
    case Texture::DXT1:
    case Texture::DXT3:
    case Texture::DXT5:
    case Texture::UNKNOWN_FORMAT:
      DCHECK(false);
      return false;
  }
  unsigned int pot_width = ComputePOTSize(width);
  unsigned int pot_height = ComputePOTSize(height);
  if (pot_width == width && pot_height == height && src == dst)
    return true;
  return Scale(
      width, height, format, src, pot_width, pot_height, dst, dst_pitch);
}

unsigned int GetNumComponentsForFormat(o3d::Texture::Format format) {
  switch (format) {
    case o3d::Texture::XRGB8:
    case o3d::Texture::ARGB8:
    case o3d::Texture::ABGR16F:
    case o3d::Texture::ABGR32F:
      return 4;
    case o3d::Texture::R32F:
      return 1;
    case o3d::Texture::DXT1:
    case o3d::Texture::DXT3:
    case o3d::Texture::DXT5:
    case o3d::Texture::UNKNOWN_FORMAT:
      break;
  }
  return 0;
}

namespace {

static const float kEpsilon = 0.0001f;
static const float kPi = 3.14159265358979f;
static const int kFilterSize = 3;

// utility function, round float numbers into 0 to 255 integers.
uint8 Safe8Round(float f) {
  f += 0.5f;
  if (f < 0.0f) {
    return 0;
  } else if (!(f < 255.0f)) {
    return 255;
  }
  return static_cast<uint8>(f);
}

template <typename T>
void PointScale(
    unsigned components,
    const uint8* src,
    unsigned src_width,
    unsigned src_height,
    uint8* dst,
    int dst_pitch,
    unsigned dst_width,
    unsigned dst_height) {
  const T* use_src = reinterpret_cast<const T*>(src);
  T* use_dst = reinterpret_cast<T*>(dst);
  int pitch = dst_pitch / sizeof(*use_src) / components;
  // Start from the end to be able to do it in place.
  for (unsigned int y = dst_height - 1; y < dst_height; --y) {
    // max value for y is dst_height - 1, which makes :
    // base_y = (2*dst_height - 1) * src_height / (2 * dst_height)
    // which is < src_height.
    unsigned int base_y = ((y * 2 + 1) * src_height) / (dst_height * 2);
    DCHECK_LT(base_y, src_height);
    for (unsigned int x = dst_width - 1; x < dst_width; --x) {
      unsigned int base_x = ((x * 2 + 1) * src_width) / (dst_width * 2);
      DCHECK_LT(base_x, src_width);
      for (unsigned int c = 0; c < components; ++c) {
        use_dst[(y * pitch + x) * components + c] =
            use_src[(base_y * src_width + base_x) * components + c];
      }
    }
  }
}

}  // anonymous namespace

// Scales the image using basic point filtering.
bool Scale(unsigned int src_width,
           unsigned int src_height,
           Texture::Format format,
           const uint8 *src,
           unsigned int dst_width,
           unsigned int dst_height,
           uint8 *dst,
           int dst_pitch) {
  DCHECK(CheckImageDimensions(src_width, src_height));
  DCHECK(CheckImageDimensions(dst_width, dst_height));
  switch (format) {
    case Texture::XRGB8:
    case Texture::ARGB8: {
      PointScale<uint8>(4, src, src_width, src_height,
                        dst, dst_pitch, dst_width, dst_height);
      break;
    }
    case Texture::ABGR16F: {
      PointScale<uint16>(4, src, src_width, src_height,
                         dst, dst_pitch, dst_width, dst_height);
      break;
    }
    case Texture::R32F:
    case Texture::ABGR32F: {
      PointScale<float>(format == Texture::R32F ? 1 : 4,
                        src, src_width, src_height,
                        dst, dst_pitch, dst_width, dst_height);
      break;
    }
    case Texture::DXT1:
    case Texture::DXT3:
    case Texture::DXT5:
    case Texture::UNKNOWN_FORMAT:
      DCHECK(false);
      return false;
  }
  return true;
}


namespace  {

// utility function called in AdjustDrawImageBoundary.
// help to adjust a specific dimension,
// if start point or ending point is out of boundary.
bool AdjustDrawImageBoundHelper(int* src_a, int* dest_a,
                                int* src_length, int* dest_length,
                                int src_bmp_length) {
  if (*src_length == 0 || *dest_length == 0)
    return false;

  // check if start point is out of boundary.
  // if src_a < 0, src_length must be positive.
  if (*src_a < 0) {
    int src_length_delta = 0 - *src_a;
    *dest_a = *dest_a + (*dest_length) * src_length_delta / (*src_length);
    *dest_length = *dest_length - (*dest_length) *
                   src_length_delta / (*src_length);
    *src_length = *src_length - src_length_delta;
    *src_a = 0;
  }
  // if src_a >= src_bmp_width, src_length must be negative.
  if (*src_a >= src_bmp_length) {
    int src_length_delta = *src_a - (src_bmp_length - 1);
    *dest_a = *dest_a - (*dest_length) * src_length_delta / (*src_length);
    *dest_length = *dest_length - (*dest_length) *
                   src_length_delta / *src_length;
    *src_length = *src_length - src_length_delta;
    *src_a = src_bmp_length - 1;
  }

  if (*src_length == 0 || *dest_length == 0)
    return false;
  // check whether start point + related length is out of boundary.
  // if src_a + src_length > src_bmp_length, src_length must be positive.
  if (*src_a + *src_length > src_bmp_length) {
    int src_length_delta = *src_length - (src_bmp_length - *src_a);
    *dest_length = *dest_length - (*dest_length) *
                   src_length_delta / (*src_length);
    *src_length = *src_length - src_length_delta;
  }
  // if src_a + src_length < -1, src_length must be negative.
  if (*src_a + *src_length < -1) {
    int src_length_delta = 0 - (*src_a + *src_length);
    *dest_length = *dest_length + (*dest_length) *
                   src_length_delta / (*src_length);
    *src_length = *src_length + src_length_delta;
  }

  return true;
}

void LanczosResize1D(const uint8* src, int src_x, int src_y,
                     int width, int height,
                     int src_bmp_width, int src_bmp_height,
                     uint8* out, int dest_pitch,
                     int dest_x, int dest_y,
                     int nwidth,
                     int dest_bmp_width, int dest_bmp_height,
                     bool isWidth, int components) {
  int pitch = dest_pitch / components;
  // calculate scale factor and init the weight array for lanczos filter.
  float scale = fabs(static_cast<float>(width) / nwidth);
  float support = kFilterSize * scale;
  scoped_array<float> weight(new float[static_cast<int>(support * 2) + 4]);
  // we assume width is the dimension we are scaling, and height stays
  // the same.
  for (int i = 0; i < abs(nwidth); ++i) {
    // center is the corresponding coordinate of i in original img.
    float center = (i + 0.5f) * scale;
    // boundary of weight array in original img.
    int xmin = static_cast<int>(floor(center - support));
    if (xmin < 0) xmin = 0;
    int xmax = static_cast<int>(ceil(center + support));
    if (xmax >= abs(width)) xmax = abs(width) - 1;

    // fill up weight array by lanczos filter.
    float wsum = 0.0;
    for (int ox = xmin; ox <= xmax; ++ox) {
      float wtemp;
      float dx = ox + 0.5f - center;
      // lanczos filter
      if (dx <= -kFilterSize || dx >= kFilterSize) {
        wtemp = 0.0;
      } else if (dx == 0.0) {
        wtemp = 1.0f;
      } else {
        wtemp = kFilterSize * sinf(kPi * dx) * sinf(kPi / kFilterSize * dx) /
                (kPi * kPi * dx * dx);
      }

      weight[ox - xmin] = wtemp;
      wsum += wtemp;
    }
    int wcount = xmax - xmin + 1;

    // Normalize the weights.
    if (fabs(wsum) > kEpsilon) {
      for (int k = 0; k < wcount; ++k) {
        weight[k] /= wsum;
      }
    }
    // Now that we've computed the filter weights for this x-position
    // of the image, we can apply that filter to all pixels in that
    // column.
    // calculate coordinate in new img.
    int x = i;
    if (nwidth < 0)
      x = -1 * x;
    // lower bound of coordinate in original img.
    if (width < 0)
      xmin = -1 * xmin;
    for (int j = 0; j < abs(height); ++j) {
      // coordinate in height, same in src and dest img.
      int base_y = j;
      if (height < 0)
        base_y = -1 * base_y;
      // TODO(yux): fix the vertical flip problem and merge this if-else
      // statement coz at that time, there would be no need to check
      // which measure we are scaling.
      if (isWidth) {
        const uint8* inrow = src + ((src_bmp_height - (src_y + base_y) - 1) *
                             src_bmp_width + src_x + xmin) * components;
        uint8* outpix = out + ((dest_bmp_height - (dest_y + base_y) - 1) *
                        pitch + dest_x + x) * components;
        int step = components;
        if (width < 0)
          step = -1 * step;
        for (int b = 0; b < components; ++b) {
          float sum = 0.0;
          for (int k = 0, xk = b; k < wcount; ++k, xk += step)
            sum += weight[k] * inrow[xk];

          outpix[b] = Safe8Round(sum);
        }
      } else {
        const uint8* inrow = src + (src_x + base_y + (src_bmp_height -
                             (src_y + xmin) - 1) * src_bmp_width) *
                             components;
        uint8* outpix = out + (dest_x + base_y + (dest_bmp_height -
                        (dest_y + x) - 1) * pitch) * components;

        int step = src_bmp_width * components;
        if (width < 0)
          step = -1 * step;
        for (int b = 0; b < components; ++b) {
          float sum = 0.0;
          for (int k = 0, xk = b; k < wcount; ++k, xk -= step)
            sum += weight[k] * inrow[xk];

          outpix[b] = Safe8Round(sum);
        }
      }
    }
  }
}

// Compute a texel, filtered from several source texels. This function assumes
// minification.
// Parameters:
//   x: x-coordinate of the destination texel in the destination image
//   y: y-coordinate of the destination texel in the destination image
//   dst_width: width of the destination image
//   dst_height: height of the destination image
//   dst_data: address of the destination image data
//   src_width: width of the source image
//   src_height: height of the source image
//   src_data: address of the source image data
//   components: number of components in the image.
template <typename OriginalType,
          typename WorkType,
          WorkType convert_to_work(OriginalType value),
          OriginalType convert_to_original(WorkType)>
void FilterTexel(unsigned int x,
                 unsigned int y,
                 unsigned int dst_width,
                 unsigned int dst_height,
                 void *dst_data,
                 int dst_pitch,
                 unsigned int src_width,
                 unsigned int src_height,
                 const void *src_data,
                 int src_pitch,
                 unsigned int components) {
  DCHECK(image::CheckImageDimensions(src_width, src_height));
  DCHECK(image::CheckImageDimensions(dst_width, dst_height));
  DCHECK_LE(dst_width, src_width);
  DCHECK_LE(dst_height, src_height);
  DCHECK_LE(x, dst_width);
  DCHECK_LE(y, dst_height);
  DCHECK_LE(static_cast<int>(src_width), src_pitch);
  DCHECK_LE(static_cast<int>(dst_width), dst_pitch);

  const OriginalType* src = static_cast<const OriginalType*>(src_data);
  OriginalType* dst = static_cast<OriginalType*>(dst_data);

  DCHECK_EQ(src_pitch % (components * sizeof(*src)), 0);
  DCHECK_EQ(dst_pitch % (components * sizeof(*dst)), 0);

  src_pitch /= components;
  dst_pitch /= components;
  // the texel at (x, y) represents the square of texture coordinates
  // [x/dst_w, (x+1)/dst_w) x [y/dst_h, (y+1)/dst_h).
  // This takes contributions from the texels:
  // [floor(x*src_w/dst_w), ceil((x+1)*src_w/dst_w)-1]
  // x
  // [floor(y*src_h/dst_h), ceil((y+1)*src_h/dst_h)-1]
  // from the previous level.
  unsigned int src_min_x = (x * src_width) / dst_width;
  unsigned int src_max_x =
      ((x + 1) * src_width + dst_width - 1) / dst_width - 1;
  unsigned int src_min_y = (y * src_height) / dst_height;
  unsigned int src_max_y =
      ((y + 1) * src_height + dst_height - 1) / dst_height - 1;

  // Find the contribution of source each texel, by computing the coverage of
  // the destination texel on the source texel. We do all the computations in
  // fixed point, at a src_height*src_width factor to be able to use ints,
  // but keep all the precision.
  // Accumulators need to be 64 bits though, because src_height*src_width can
  // be 24 bits for a 4kx4k base, to which we need to multiply the component
  // value which is another 8 bits (and we need to accumulate several of them).

  // NOTE: all of our formats use at most 4 components per pixel.
  // Instead of dynamically allocating a buffer for each pixel on the heap,
  // just allocate the worst case on the stack.
  DCHECK_LE(components, 4u);
  WorkType accum[4] = {0};
  for (unsigned int src_x = src_min_x; src_x <= src_max_x; ++src_x) {
    for (unsigned int src_y = src_min_y; src_y <= src_max_y; ++src_y) {
      // The contribution of a fully covered texel is 1/(m_x*m_y) where m_x is
      // the x-dimension minification factor (src_width/dst_width) and m_y is
      // the y-dimenstion minification factor (src_height/dst_height).
      // If the texel is partially covered (on a border), the contribution is
      // proportional to the covered area. We compute it as the product of the
      // covered x-length by the covered y-length.

      unsigned int x_contrib = dst_width;
      if (src_x * dst_width < x * src_width) {
        // source texel is across the left border of the footprint of the
        // destination texel.
        x_contrib = (src_x + 1) * dst_width - x * src_width;
      } else if ((src_x + 1) * dst_width > (x+1) * src_width) {
        // source texel is across the right border of the footprint of the
        // destination texel.
        x_contrib = (x+1) * src_width - src_x * dst_width;
      }
      DCHECK(x_contrib > 0);
      DCHECK(x_contrib <= dst_width);
      unsigned int y_contrib = dst_height;
      if (src_y * dst_height < y * src_height) {
        // source texel is across the top border of the footprint of the
        // destination texel.
        y_contrib = (src_y + 1) * dst_height - y * src_height;
      } else if ((src_y + 1) * dst_height > (y+1) * src_height) {
        // source texel is across the bottom border of the footprint of the
        // destination texel.
        y_contrib = (y+1) * src_height - src_y * dst_height;
      }
      DCHECK(y_contrib > 0);
      DCHECK(y_contrib <= dst_height);
      WorkType contrib = static_cast<WorkType>(x_contrib * y_contrib);
      for (unsigned int c = 0; c < components; ++c) {
        accum[c] += contrib *
          convert_to_work(src[(src_y * src_pitch + src_x) * components + c]);
      }
    }
  }
  for (unsigned int c = 0; c < components; ++c) {
    WorkType value = accum[c] / static_cast<WorkType>(src_height * src_width);
    dst[(y * dst_pitch + x) * components + c] = convert_to_original(value);
  }
}

template <typename OriginalType,
          typename WorkType,
          typename FilterType,
          WorkType convert_to_work(OriginalType value),
          OriginalType convert_from_work(WorkType),
          FilterType convert_to_filter(OriginalType value),
          OriginalType convert_from_filter(FilterType)>
void GenerateMip(unsigned int components,
                 unsigned int src_width,
                 unsigned int src_height,
                 const void *src_data,
                 int src_pitch,
                 void *dst_data,
                 int dst_pitch) {
  unsigned int mip_width = std::max(1U, src_width >> 1);
  unsigned int mip_height = std::max(1U, src_height >> 1);

  const OriginalType* src = static_cast<const OriginalType*>(src_data);
  OriginalType* dst = static_cast<OriginalType*>(dst_data);

  if (mip_width * 2 == src_width && mip_height * 2 == src_height) {
    DCHECK_EQ(src_pitch % (components * sizeof(*src)), 0);
    DCHECK_EQ(dst_pitch % (components * sizeof(*dst)), 0);
    src_pitch /= components;
    dst_pitch /= components;
    // Easy case: every texel maps to exactly 4 texels in the previous level.
    for (unsigned int y = 0; y < mip_height; ++y) {
      for (unsigned int x = 0; x < mip_width; ++x) {
        for (unsigned int c = 0; c < components; ++c) {
          // Average the 4 texels.
          unsigned int offset = (y * 2 * src_pitch + x * 2) * components + c;
          WorkType value = convert_to_work(src[offset]);        // (2x, 2y)
          value += convert_to_work(src[offset + components]);   // (2x+1, 2y)
          value += convert_to_work(src[offset + src_width * components]);
                                                                // (2x, 2y+1)
          value += convert_to_work(src[offset + (src_width + 1) * components]);
                                                                // (2x+1, 2y+1)
          dst[(y * dst_pitch + x) * components + c] =
              convert_from_work(value / static_cast<WorkType>(4));
        }
      }
    }
  } else {
    for (unsigned int y = 0; y < mip_height; ++y) {
      for (unsigned int x = 0; x < mip_width; ++x) {
        FilterTexel<OriginalType,
                    FilterType,
                    convert_to_filter,
                    convert_from_filter>(
            x, y, mip_width, mip_height, dst_data, dst_pitch,
            src_width, src_height, src_data, src_pitch, components);
      }
    }
  }
}

uint32 UInt8ToUInt32(uint8 value) {
  return static_cast<uint32>(value);
};

uint8 UInt32ToUInt8(uint32 value) {
  return static_cast<uint8>(value);
};

uint64 UInt8ToUInt64(uint8 value) {
  return static_cast<uint64>(value);
};

uint8 UInt64ToUInt8(uint64 value) {
  return static_cast<uint8>(value);
};

float FloatToFloat(float value) {
  return value;
}

double FloatToDouble(float value) {
  return static_cast<double>(value);
}

float DoubleToFloat(double value) {
  return static_cast<float>(value);
}

float HalfToFloat(uint16 value) {
    return Vectormath::Aos::HalfToFloat(value);
}

uint16 FloatToHalf(float value) {
    return Vectormath::Aos::FloatToHalf(value);
}

double HalfToDouble(uint16 value) {
    return static_cast<double>(Vectormath::Aos::HalfToFloat(value));
}

uint16 DoubleToHalf(double value) {
    return Vectormath::Aos::FloatToHalf(static_cast<float>(value));
}

}  // anonymous namespace

// Adjust boundaries when using DrawImage function in bitmap or texture.
bool AdjustDrawImageBoundary(int* src_x, int* src_y,
                             int* src_width, int* src_height,
                             int src_bmp_width, int src_bmp_height,
                             int* dest_x, int* dest_y,
                             int* dest_width, int* dest_height,
                             int dest_bmp_width, int dest_bmp_height) {
  // if src or dest rectangle is out of boundaries, do nothing.
  if ((*src_x < 0 && *src_x + *src_width <= 0) ||
      (*src_y < 0 && *src_y + *src_height <= 0) ||
      (*dest_x < 0 && *dest_x + *dest_width <= 0) ||
      (*dest_y < 0 && *dest_y + *dest_height <= 0) ||
      (*src_x >= src_bmp_width &&
       *src_x + *src_width >= src_bmp_width - 1) ||
      (*src_y >= src_bmp_height &&
       *src_y + *src_height >= src_bmp_height - 1) ||
      (*dest_x >= dest_bmp_width &&
       *dest_x + *dest_width >= dest_bmp_width - 1) ||
      (*dest_y >= dest_bmp_height &&
       *dest_y + *dest_height >= dest_bmp_height - 1))
    return false;

  // if start points are negative.
  // check whether src_x is negative.
  if (!AdjustDrawImageBoundHelper(src_x, dest_x,
                                  src_width, dest_width, src_bmp_width))
    return false;
  // check whether dest_x is negative.
  if (!AdjustDrawImageBoundHelper(dest_x, src_x,
                                  dest_width, src_width, dest_bmp_width))
    return false;
  // check whether src_y is negative.
  if (!AdjustDrawImageBoundHelper(src_y, dest_y,
                                  src_height, dest_height, src_bmp_height))
    return false;
  // check whether dest_y is negative.
  if (!AdjustDrawImageBoundHelper(dest_y, src_y,
                                  dest_height, src_height, dest_bmp_height))
    return false;

  // check any width or height becomes negative after adjustment.
  if (*src_width == 0 || *src_height == 0 ||
      *dest_width == 0 || *dest_height == 0) {
    return false;
  }

  return true;
}

void LanczosScale(const uint8* src,
                  int src_x, int src_y,
                  int src_width, int src_height,
                  int src_img_width, int src_img_height,
                  uint8* dest, int dest_pitch,
                  int dest_x, int dest_y,
                  int dest_width, int dest_height,
                  int dest_img_width, int dest_img_height,
                  int components) {
  // Scale the image horizontally to a temp buffer.
  int temp_img_width = abs(dest_width);
  int temp_img_height = abs(src_height);
  int temp_width = dest_width;
  int temp_height = src_height;
  int temp_x = 0, temp_y = 0;
  if (temp_width < 0)
    temp_x = abs(temp_width) - 1;
  if (temp_height < 0)
    temp_y = abs(temp_height) - 1;

  scoped_array<uint8> temp(new uint8[temp_img_width *
                                     temp_img_height * components]);

  LanczosResize1D(src, src_x, src_y, src_width, src_height,
                  src_img_width, src_img_height,
                  temp.get(), temp_img_width * components,
                  temp_x, temp_y, temp_width,
                  temp_img_width, temp_img_height, true, components);

  // Scale the temp buffer vertically to get the final result.
  LanczosResize1D(temp.get(), temp_x, temp_y, temp_height, temp_width,
                  temp_img_width, temp_img_height,
                  dest, dest_pitch,
                  dest_x, dest_y, dest_height,
                  dest_img_width, dest_img_height, false, components);
}

bool GenerateMipmap(unsigned int src_width,
                    unsigned int src_height,
                    Texture::Format format,
                    const uint8 *src_data,
                    int src_pitch,
                    uint8 *dst_data,
                    int dst_pitch) {
  unsigned int components = GetNumComponentsForFormat(format);
  if (components == 0) {
    DLOG(ERROR) << "Mip-map generation not supported for format: " << format;
    return false;
  }

  switch (format) {
    case Texture::ARGB8:
    case Texture::XRGB8:
      GenerateMip<uint8, uint32, uint64,
                  UInt8ToUInt32, UInt32ToUInt8,
                  UInt8ToUInt64, UInt64ToUInt8>(
        components, src_width, src_height, src_data, src_pitch,
        dst_data, dst_pitch);
      break;
    case Texture::ABGR16F:
      GenerateMip<uint16, float, double,
                  HalfToFloat, FloatToHalf,
                  HalfToDouble, DoubleToHalf>(
        components, src_width, src_height, src_data, src_pitch,
        dst_data, dst_pitch);
      break;
    case Texture::ABGR32F:
    case Texture::R32F:
      GenerateMip<float, float, double,
                  FloatToFloat, FloatToFloat,
                  FloatToDouble, DoubleToFloat>(
        components, src_width, src_height, src_data, src_pitch,
        dst_data, dst_pitch);
      break;
  }
  return true;
}

ImageFileType GetFileTypeFromFilename(const char *filename) {
  // Convert the filename to lower case for matching.
  // NOTE: Surprisingly, "tolower" is not in the std namespace.
  String name(filename);
  std::transform(name.begin(), name.end(), name.begin(), ::tolower);

  // Dispatch loading functions based on filename extensions.
  String::size_type i = name.rfind(".");
  if (i == String::npos) {
    DLOG(INFO) << "Could not detect file type for image \""
               << filename << "\": no extension.";
    return UNKNOWN;
  }

  String extension = name.substr(i);
  if (extension == ".tga") {
    DLOG(INFO) << "Bitmap Found a TGA file : " << filename;
    return TGA;
  } else if (extension == ".dds") {
    DLOG(INFO) << "Bitmap Found a DDS file : " << filename;
    return DDS;
  } else if (extension == ".png") {
    DLOG(INFO) << "Bitmap Found a PNG file : " << filename;
    return PNG;
  } else if (extension == ".jpg" ||
             extension == ".jpeg" ||
             extension == ".jpe") {
    DLOG(INFO) << "Bitmap Found a JPEG file : " << filename;
    return JPEG;
  } else {
    return UNKNOWN;
  }
}

namespace {

struct MimeTypeToFileType {
  const char *mime_type;
  ImageFileType file_type;
};

const MimeTypeToFileType mime_type_map[] = {
  {"image/png", PNG},
  {"image/jpeg", JPEG},
  // No official MIME type for TGA or DDS.
};

}  // anonymous namespace

ImageFileType GetFileTypeFromMimeType(const char *mime_type) {
  for (unsigned int i = 0u; i < arraysize(mime_type_map); ++i) {
    if (!strcmp(mime_type, mime_type_map[i].mime_type))
      return mime_type_map[i].file_type;
  }
  return UNKNOWN;
}

void XYZToXYZA(uint8 *image_data, int pixel_count) {
  // We do this pixel by pixel, starting from the end to avoid overlapping
  // problems.
  for (int i = pixel_count - 1; i >= 0; --i) {
    image_data[i * 4 + 3] = 0xff;
    image_data[i * 4 + 2] = image_data[i * 3 + 2];
    image_data[i * 4 + 1] = image_data[i * 3 + 1];
    image_data[i * 4 + 0] = image_data[i * 3 + 0];
  }
}

void RGBAToBGRA(uint8 *image_data, int pixel_count) {
  for (int i = 0; i < pixel_count; ++i) {
    uint8 c = image_data[i * 4 + 0];
    image_data[i * 4 + 0] = image_data[i * 4 + 2];
    image_data[i * 4 + 2] = c;
  }
}

}  // namespace image
}  // namespace o3d


