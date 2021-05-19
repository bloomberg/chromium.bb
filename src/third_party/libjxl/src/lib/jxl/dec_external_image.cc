// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "lib/jxl/dec_external_image.h"

#include <string.h>

#include <algorithm>
#include <array>
#include <functional>
#include <utility>
#include <vector>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/dec_external_image.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/alpha.h"
#include "lib/jxl/base/byte_order.h"
#include "lib/jxl/base/cache_aligned.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/color_management.h"
#include "lib/jxl/common.h"
#include "lib/jxl/transfer_functions-inl.h"

HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

// Input/output uses the codec.h scaling: nominally 0-1 if in-gamut.
template <class V>
V LinearToSRGB(V encoded) {
  return TF_SRGB().EncodedFromDisplay(HWY_FULL(float)(), encoded);
}

void LinearToSRGBInPlace(jxl::ThreadPool* pool, Image3F* image,
                         size_t color_channels) {
  size_t xsize = image->xsize();
  size_t ysize = image->ysize();
  const HWY_FULL(float) d;
  for (size_t c = 0; c < color_channels; ++c) {
    RunOnPool(
        pool, 0, static_cast<uint32_t>(ysize), ThreadPool::SkipInit(),
        [&](const int task, int /*thread*/) {
          const int64_t y = task;
          float* JXL_RESTRICT row = image->PlaneRow(c, y);
          for (size_t x = 0; x < xsize; x += Lanes(d)) {
            const auto v = LinearToSRGB(Load(d, row + x));
            Store(v, d, row + x);
          }
        },
        "LinearToSRGB");
  }
}

void FloatToU32(const float* in, uint32_t* out, size_t num, float mul,
                size_t bits_per_sample) {
  const HWY_FULL(float) d;
  const hwy::HWY_NAMESPACE::Rebind<uint32_t, HWY_FULL(float)> du;
  size_t vec_num = num;
  if (bits_per_sample == 32) {
    // Conversion to real 32-bit *unsigned* integers requires more intermediate
    // precision that what is given by the usual f32 -> i32 conversion
    // instructions, so we run the non-SIMD path for those.
    vec_num = 0;
  } else if (JXL_IS_DEBUG_BUILD) {
    // Avoid accessing partially-uninitialized vectors with memory sanitizer.
    vec_num &= ~(Lanes(d) - 1);
  }
  const auto one = Set(d, 1.0f);
  const auto scale = Set(d, mul);
  for (size_t x = 0; x < vec_num; x += Lanes(d)) {
    auto v = Load(d, in + x);
    // Check for NaNs.
    JXL_DASSERT(AllTrue(v == v));
    // Clamp turns NaN to 'min'.
    v = Clamp(v, Zero(d), one);
    auto i = NearestInt(v * scale);
    Store(BitCast(du, i), du, out + x);
  }
  for (size_t x = vec_num; x < num; x++) {
    float v = in[x];
    JXL_DASSERT(!std::isnan(v));
    // Inverted condition grants that NaN is mapped to 0.0f.
    v = (v >= 0.0f) ? (v > 1.0f ? mul : (v * mul)) : 0.0f;
    out[x] = static_cast<uint32_t>(v + 0.5f);
  }
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace jxl {
namespace {

// Stores a float in big endian
void StoreBEFloat(float value, uint8_t* p) {
  uint32_t u;
  memcpy(&u, &value, 4);
  StoreBE32(u, p);
}

// Stores a float in little endian
void StoreLEFloat(float value, uint8_t* p) {
  uint32_t u;
  memcpy(&u, &value, 4);
  StoreLE32(u, p);
}

// The orientation may not be identity.
// TODO(lode): SIMDify where possible
template <typename T>
void UndoOrientation(jxl::Orientation undo_orientation, const Plane<T>& image,
                     Plane<T>& out, jxl::ThreadPool* pool) {
  const size_t xsize = image.xsize();
  const size_t ysize = image.ysize();

  if (undo_orientation == Orientation::kFlipHorizontal) {
    out = Plane<T>(xsize, ysize);
    RunOnPool(
        pool, 0, static_cast<uint32_t>(ysize), ThreadPool::SkipInit(),
        [&](const int task, int /*thread*/) {
          const int64_t y = task;
          const T* JXL_RESTRICT row_in = image.Row(y);
          T* JXL_RESTRICT row_out = out.Row(y);
          for (size_t x = 0; x < xsize; ++x) {
            row_out[xsize - x - 1] = row_in[x];
          }
        },
        "UndoOrientation");
  } else if (undo_orientation == Orientation::kRotate180) {
    out = Plane<T>(xsize, ysize);
    RunOnPool(
        pool, 0, static_cast<uint32_t>(ysize), ThreadPool::SkipInit(),
        [&](const int task, int /*thread*/) {
          const int64_t y = task;
          const T* JXL_RESTRICT row_in = image.Row(y);
          T* JXL_RESTRICT row_out = out.Row(ysize - y - 1);
          for (size_t x = 0; x < xsize; ++x) {
            row_out[xsize - x - 1] = row_in[x];
          }
        },
        "UndoOrientation");
  } else if (undo_orientation == Orientation::kFlipVertical) {
    out = Plane<T>(xsize, ysize);
    RunOnPool(
        pool, 0, static_cast<uint32_t>(ysize), ThreadPool::SkipInit(),
        [&](const int task, int /*thread*/) {
          const int64_t y = task;
          const T* JXL_RESTRICT row_in = image.Row(y);
          T* JXL_RESTRICT row_out = out.Row(ysize - y - 1);
          for (size_t x = 0; x < xsize; ++x) {
            row_out[x] = row_in[x];
          }
        },
        "UndoOrientation");
  } else if (undo_orientation == Orientation::kTranspose) {
    out = Plane<T>(ysize, xsize);
    RunOnPool(
        pool, 0, static_cast<uint32_t>(ysize), ThreadPool::SkipInit(),
        [&](const int task, int /*thread*/) {
          const int64_t y = task;
          const T* JXL_RESTRICT row_in = image.Row(y);
          for (size_t x = 0; x < xsize; ++x) {
            out.Row(x)[y] = row_in[x];
          }
        },
        "UndoOrientation");
  } else if (undo_orientation == Orientation::kRotate90) {
    out = Plane<T>(ysize, xsize);
    RunOnPool(
        pool, 0, static_cast<uint32_t>(ysize), ThreadPool::SkipInit(),
        [&](const int task, int /*thread*/) {
          const int64_t y = task;
          const T* JXL_RESTRICT row_in = image.Row(y);
          for (size_t x = 0; x < xsize; ++x) {
            out.Row(x)[ysize - y - 1] = row_in[x];
          }
        },
        "UndoOrientation");
  } else if (undo_orientation == Orientation::kAntiTranspose) {
    out = Plane<T>(ysize, xsize);
    RunOnPool(
        pool, 0, static_cast<uint32_t>(ysize), ThreadPool::SkipInit(),
        [&](const int task, int /*thread*/) {
          const int64_t y = task;
          const T* JXL_RESTRICT row_in = image.Row(y);
          for (size_t x = 0; x < xsize; ++x) {
            out.Row(xsize - x - 1)[ysize - y - 1] = row_in[x];
          }
        },
        "UndoOrientation");
  } else if (undo_orientation == Orientation::kRotate270) {
    out = Plane<T>(ysize, xsize);
    RunOnPool(
        pool, 0, static_cast<uint32_t>(ysize), ThreadPool::SkipInit(),
        [&](const int task, int /*thread*/) {
          const int64_t y = task;
          const T* JXL_RESTRICT row_in = image.Row(y);
          for (size_t x = 0; x < xsize; ++x) {
            out.Row(xsize - x - 1)[y] = row_in[x];
          }
        },
        "UndoOrientation");
  }
}
}  // namespace

HWY_EXPORT(LinearToSRGBInPlace);
HWY_EXPORT(FloatToU32);

namespace {

using StoreFuncType = void(uint32_t value, uint8_t* dest);
template <StoreFuncType StoreFunc>
void StoreUintRow(uint32_t* JXL_RESTRICT* rows_u32, size_t num_channels,
                  size_t xsize, size_t bytes_per_sample,
                  uint8_t* JXL_RESTRICT out) {
  for (size_t x = 0; x < xsize; ++x) {
    for (size_t c = 0; c < num_channels; c++) {
      StoreFunc(rows_u32[c][x],
                out + (num_channels * x + c) * bytes_per_sample);
    }
  }
}

template <void(StoreFunc)(float, uint8_t*)>
void StoreFloatRow(const float* JXL_RESTRICT* rows_in, size_t num_channels,
                   size_t xsize, uint8_t* JXL_RESTRICT out) {
  for (size_t x = 0; x < xsize; ++x) {
    for (size_t c = 0; c < num_channels; c++) {
      StoreFunc(rows_in[c][x], out + (num_channels * x + c) * sizeof(float));
    }
  }
}

void JXL_INLINE Store8(uint32_t value, uint8_t* dest) { *dest = value & 0xff; }

}  // namespace

void LinearToSRGBInPlace(jxl::ThreadPool* pool, Image3F* image,
                         size_t color_channels) {
  return HWY_DYNAMIC_DISPATCH(LinearToSRGBInPlace)(pool, image, color_channels);
}

Status ConvertToExternal(const jxl::ImageBundle& ib, size_t bits_per_sample,
                         bool float_out, bool apply_srgb_tf,
                         size_t num_channels, JxlEndianness endianness,
                         size_t stride, jxl::ThreadPool* pool, void* out_image,
                         size_t out_size, jxl::Orientation undo_orientation) {
  if (bits_per_sample < 1 || bits_per_sample > 32) {
    return JXL_FAILURE("Invalid bits_per_sample value.");
  }
  // TODO(deymo): Implement 1-bit per pixel packed in 8 samples per byte.
  if (bits_per_sample == 1) {
    return JXL_FAILURE("packed 1-bit per sample is not yet supported");
  }
  size_t xsize = ib.xsize();
  size_t ysize = ib.ysize();

  uint8_t* out = reinterpret_cast<uint8_t*>(out_image);

  bool want_alpha = num_channels == 2 || num_channels == 4;
  size_t color_channels = num_channels <= 2 ? 1 : 3;

  // bytes_per_channel and is only valid for bits_per_sample > 1.
  const size_t bytes_per_channel = DivCeil(bits_per_sample, jxl::kBitsPerByte);
  const size_t bytes_per_pixel = num_channels * bytes_per_channel;

  const Image3F* color = &ib.color();
  Image3F temp_color;
  const ImageF* alpha = ib.HasAlpha() ? &ib.alpha() : nullptr;
  ImageF temp_alpha;
  if (apply_srgb_tf) {
    temp_color = CopyImage(*color);
    LinearToSRGBInPlace(pool, &temp_color, color_channels);
    color = &temp_color;
  }

  if (undo_orientation != Orientation::kIdentity) {
    Image3F transformed;
    for (size_t c = 0; c < color_channels; ++c) {
      UndoOrientation(undo_orientation, color->Plane(c), transformed.Plane(c),
                      pool);
    }
    transformed.Swap(temp_color);
    color = &temp_color;
    if (ib.HasAlpha()) {
      UndoOrientation(undo_orientation, *alpha, temp_alpha, pool);
      alpha = &temp_alpha;
    }

    xsize = color->xsize();
    ysize = color->ysize();
  }

  if (stride < bytes_per_pixel * xsize) {
    return JXL_FAILURE(
        "stride is smaller than scanline width in bytes: %zu vs %zu", stride,
        bytes_per_pixel * xsize);
  }

  const bool little_endian =
      endianness == JXL_LITTLE_ENDIAN ||
      (endianness == JXL_NATIVE_ENDIAN && IsLittleEndian());

  ImageF ones;
  if (want_alpha && !ib.HasAlpha()) {
    ones = ImageF(xsize, 1);
    FillImage(1.0f, &ones);
  }

  if (float_out) {
    if (bits_per_sample != 32) {
      return JXL_FAILURE("non-32-bit float not supported");
    }
    RunOnPool(
        pool, 0, static_cast<uint32_t>(ysize), ThreadPool::SkipInit(),
        [&](const int task, int /*thread*/) {
          const int64_t y = task;
          size_t i = stride * y;
          const float* JXL_RESTRICT row_in[4];
          size_t c = 0;
          for (; c < color_channels; c++) {
            row_in[c] = color->PlaneRow(c, y);
          }
          if (want_alpha) {
            row_in[c++] = ib.HasAlpha() ? alpha->Row(y) : ones.Row(0);
          }
          JXL_ASSERT(c == num_channels);
          if (little_endian) {
            StoreFloatRow<StoreLEFloat>(row_in, c, xsize, out + i);
          } else {
            StoreFloatRow<StoreBEFloat>(row_in, c, xsize, out + i);
          }
        },
        "ConvertFloat");
  } else {
    // Multiplier to convert from floating point 0-1 range to the integer
    // range.
    float mul = (1ull << bits_per_sample) - 1;
    Plane<uint32_t> u32_cache;
    RunOnPool(
        pool, 0, static_cast<uint32_t>(ysize),
        [&](size_t num_threads) {
          u32_cache = Plane<uint32_t>(xsize, num_channels * num_threads);
          return true;
        },
        [&](const int task, int thread) {
          const int64_t y = task;
          size_t i = stride * y;
          const float* JXL_RESTRICT row_in[4];
          size_t c = 0;
          for (; c < color_channels; c++) {
            row_in[c] = color->PlaneRow(c, y);
          }
          if (want_alpha) {
            row_in[c++] = ib.HasAlpha() ? alpha->Row(y) : ones.Row(0);
          }
          JXL_ASSERT(c == num_channels);
          uint32_t* JXL_RESTRICT row_u32[4];
          for (size_t r = 0; r < c; r++) {
            row_u32[r] = u32_cache.Row(r + thread * num_channels);
            HWY_DYNAMIC_DISPATCH(FloatToU32)
            (row_in[r], row_u32[r], xsize, mul, bits_per_sample);
          }
          // TODO(deymo): add bits_per_sample == 1 case here.
          if (bits_per_sample <= 8) {
            StoreUintRow<Store8>(row_u32, c, xsize, 1, out + i);
          } else if (bits_per_sample <= 16) {
            if (little_endian) {
              StoreUintRow<StoreLE16>(row_u32, c, xsize, 2, out + i);
            } else {
              StoreUintRow<StoreBE16>(row_u32, c, xsize, 2, out + i);
            }
          } else if (bits_per_sample <= 24) {
            if (little_endian) {
              StoreUintRow<StoreLE24>(row_u32, c, xsize, 3, out + i);
            } else {
              StoreUintRow<StoreBE24>(row_u32, c, xsize, 3, out + i);
            }
          } else {
            if (little_endian) {
              StoreUintRow<StoreLE32>(row_u32, c, xsize, 4, out + i);
            } else {
              StoreUintRow<StoreBE32>(row_u32, c, xsize, 4, out + i);
            }
          }
        },
        "ConvertUint");
  }

  return true;
}

}  // namespace jxl
#endif  // HWY_ONCE
