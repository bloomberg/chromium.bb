/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "aom_mem/aom_mem.h"
#include "av1/encoder/rdopt.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace {

class GaussianBlurTest : public ::testing::Test {};

/** Get the (x, y) value from the input; if i or j is outside of the width
 * or height, the nearest pixel value is returned.
 */
static uint8_t get_xy(const uint8_t *data, int w, int h, int i, int j) {
  return data[AOMMAX(AOMMIN(i, w - 1), 0) + w * AOMMAX(AOMMIN(j, h - 1), 0)];
}

/** Given the image data, creates a new image with padded values, so an
 * 8-tap filter can be convolved. The padded value is the same as the closest
 * value in the image. Returns a pointer to the start of the image in the
 * padded data. Must be freed with free_for_convolve.
 */
uint8_t *pad_8tap_convolve(const uint8_t *data, int w, int h) {
  // AVX2 optimizations require the width to be a multiple of 8 and the height
  // a multiple of 2.
  assert(w % 8 == 0);
  assert(h % 2 == 0);
  // For an 8-tap filter, we need to pad with 3 lines on top and on the left,
  // and 4 lines on the right and bottom, for 7 extra lines.
  const int pad_w = w + 7;
  const int pad_h = h + 7;
  uint8_t *dst = (uint8_t *)aom_memalign(32, pad_w * pad_h);
  // Fill in the data from the original.
  for (int i = 0; i < pad_w; ++i) {
    for (int j = 0; j < pad_h; ++j) {
      dst[i + j * pad_w] = get_xy(data, w, h, i - 3, j - 3);
    }
  }
  return dst + (w + 7) * 3 + 3;
}

static void free_pad_8tap(uint8_t *padded, int width) {
  aom_free(padded - (width + 7) * 3 - 3);
}

static int stride_8tap(int width) { return width + 7; }

TEST(GaussianBlurTest, UniformBrightness) {
  // Generate images ranging in size from 8x8 to 32x32, with
  // varying levels of brightness. In all cases, the algorithm should
  // produce the same output.
  // Note that width must increment in values of 8 for the AVX2 code to work.
  for (int width = 8; width <= 32; width += 8) {
    // Note that height must be even for the AVX2 code to work.
    for (int height = 4; height <= 10; height += 2) {
      for (int brightness = 0; brightness < 255; ++brightness) {
        uint8_t *orig = (uint8_t *)malloc(width * height);
        for (int i = 0; i < width * height; ++i) {
          orig[i] = brightness;
        }
        uint8_t *padded = pad_8tap_convolve(orig, width, height);
        free(orig);
        uint8_t *output = (uint8_t *)aom_memalign(32, width * height);
        gaussian_blur(padded, stride_8tap(width), width, height, output);
        for (int i = 0; i < width * height; ++i) {
          ASSERT_EQ(brightness, output[i]);
        }
        free_pad_8tap(padded, width);
        aom_free(output);
      }
    }
  }
}

TEST(GaussianBlurTest, SimpleExample) {
  // Randomly generated 8x2.
  const uint8_t luma[16] = { 241, 147, 7,  90,  184, 103, 28,  186,
                             2,   248, 49, 242, 114, 146, 127, 22 };
  uint8_t expected[] = { 151, 132, 117, 119, 124, 117, 109, 113,
                         111, 124, 129, 135, 135, 122, 103, 88 };
  const int w = 8;
  const int h = 2;
  uint8_t *padded = pad_8tap_convolve(luma, w, h);
  uint8_t *output = (uint8_t *)aom_memalign(32, w * h);
  gaussian_blur(padded, stride_8tap(w), w, h, output);

  for (int i = 0; i < w * h; ++i) {
    ASSERT_EQ(expected[i], output[i]);
  }

  free_pad_8tap(padded, w);
  aom_free(output);
}

class EdgeDetectTest : public ::testing::Test {};

TEST(EdgeDetectTest, UniformBrightness) {
  // Generate images ranging in size from 8x2 to 32x32, with
  // varying levels of brightness. In all cases, the algorithm should
  // produce the same output.
  for (int width = 8; width <= 32; width += 8) {
    for (int height = 2; height <= 32; height += 2) {
      for (int brightness = 0; brightness < 255; ++brightness) {
        uint8_t *orig = (uint8_t *)malloc(width * height);
        for (int i = 0; i < width * height; ++i) {
          orig[i] = brightness;
        }
        uint8_t *padded = pad_8tap_convolve(orig, width, height);
        free(orig);
        ASSERT_EQ(0,
                  av1_edge_exists(padded, stride_8tap(width), width, height));
        free_pad_8tap(padded, width);
      }
    }
  }
}

// Generate images ranging in size from 8x2 to 32x32, black on one side
// and white on the other.
TEST(EdgeDetectTest, BlackWhite) {
  for (int width = 8; width <= 32; width += 8) {
    for (int height = 2; height <= 32; height += 2) {
      uint8_t *orig = (uint8_t *)malloc(width * height);
      for (int i = 0; i < width; ++i) {
        for (int j = 0; j < height; ++j) {
          if (i < width / 2) {
            orig[i + j * width] = 0;
          } else {
            orig[i + j * width] = 255;
          }
        }
      }
      uint8_t *padded = pad_8tap_convolve(orig, width, height);
      free(orig);
      if (height < 3) {
        ASSERT_EQ(0,
                  av1_edge_exists(padded, stride_8tap(width), width, height));
      } else {
        ASSERT_LE(556,
                  av1_edge_exists(padded, stride_8tap(width), width, height));
      }
      free_pad_8tap(padded, width);
    }
  }
}

}  // namespace
