// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/timer/elapsed_timer.h"
#include "platform/image-encoders/RGBAtoRGB.h"
#include "platform/wtf/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class RGBAtoRGBTest : public ::testing::Test {
 public:
  RGBAtoRGBTest() {}
};

static const size_t kChannelsRGBA = 4;
static const size_t kChannelsRGB = 3;

inline size_t CalculateRGBAPixels(size_t input_buffer_size) {
  size_t pixels = input_buffer_size / kChannelsRGBA;
  return pixels;
}

inline size_t CalculateRGBOutputSize(size_t input_buffer_size) {
  size_t pixels = CalculateRGBAPixels(input_buffer_size);
  pixels *= kChannelsRGB;
  return pixels;
}

TEST_F(RGBAtoRGBTest, testOpaqueCaseEven8pixels) {
  unsigned char canvas[] = {255, 0,   0, 255, 255, 0,   0,   255, 255, 0,  0,
                            255, 255, 0, 0,   255, 0,   255, 0,   255, 0,  255,
                            0,   255, 0, 255, 0,   255, 0,   255, 0,   255};

  unsigned char expected[] = {255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 0,
                              255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0};
// TODO(dcheng): Make this all constexpr.
#if OS(WIN)
  // Windows release bot can't be reasoned with (compiler error C2131).
  static const constexpr size_t pixels = sizeof(canvas) / kChannelsRGBA;
  static const constexpr size_t rgb_size = pixels * kChannelsRGB;
#else
  const size_t pixels = CalculateRGBAPixels(sizeof(canvas));
  const size_t rgb_size = CalculateRGBOutputSize(sizeof(canvas));
#endif

  unsigned char output[rgb_size];
  memset(output, 0, rgb_size);

  blink::RGBAtoRGB(canvas, static_cast<unsigned>(pixels), output);

  EXPECT_EQ(memcmp(expected, output, rgb_size), 0);
}

#ifdef __ARM_NEON__
TEST_F(RGBAtoRGBTest, testCaseEven16pixels) {
  unsigned char canvas[] = {
      255, 0,   0,   255, 255, 0,   0,   255, 255, 0,   0,   255, 255,
      0,   0,   255, 0,   255, 0,   255, 0,   255, 0,   255, 0,   255,
      0,   255, 0,   255, 0,   255, 0,   0,   255, 128, 0,   0,   255,
      128, 0,   0,   255, 128, 0,   0,   255, 128, 128, 128, 128, 128,
      128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128};

  const size_t pixels = CalculateRGBAPixels(sizeof(canvas));
  const size_t rgb_size = CalculateRGBOutputSize(sizeof(canvas));
  unsigned char output[rgb_size];
  unsigned char expected[rgb_size];
  memset(output, 0, rgb_size);
  memset(expected, 0, rgb_size);

  blink::RGBAtoRGBScalar(canvas, static_cast<unsigned>(pixels), expected);
  blink::RGBAtoRGBNeon(canvas, static_cast<unsigned>(pixels), output);

  EXPECT_EQ(memcmp(expected, output, rgb_size), 0);
}

TEST_F(RGBAtoRGBTest, testCaseOdd17pixels) {
  unsigned char canvas[] = {
      255, 0,   0,   255, 255, 0,   0,   255, 255, 0,   0,   255, 255, 0,
      0,   255, 0,   255, 0,   255, 0,   255, 0,   255, 0,   255, 0,   255,
      0,   255, 0,   255, 0,   0,   255, 128, 0,   0,   255, 128, 0,   0,
      255, 128, 0,   0,   255, 128, 128, 128, 128, 128, 128, 128, 128, 128,
      128, 128, 128, 128, 128, 128, 128, 128, 10,  10,  10,  100};

  const size_t pixels = CalculateRGBAPixels(sizeof(canvas));
  const size_t rgb_size = CalculateRGBOutputSize(sizeof(canvas));
  unsigned char output[rgb_size];
  unsigned char expected[rgb_size];
  memset(output, 0, rgb_size);
  memset(expected, 0, rgb_size);

  blink::RGBAtoRGBScalar(canvas, static_cast<unsigned>(pixels), expected);
  blink::RGBAtoRGBNeon(canvas, static_cast<unsigned>(pixels), output);

  EXPECT_EQ(memcmp(expected, output, rgb_size), 0);
}

TEST_F(RGBAtoRGBTest, testCaseEven32pixels) {
  unsigned char canvas[] = {
      255, 0,   0,   255, 255, 0,   0,   255, 255, 0,   0,   255, 255, 0,   0,
      255, 0,   255, 0,   255, 0,   255, 0,   255, 0,   255, 0,   255, 0,   255,
      0,   255, 0,   0,   255, 128, 0,   0,   255, 128, 0,   0,   255, 128, 0,
      0,   255, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
      128, 128, 128, 128, 128, 128, 128, 255, 128, 128, 128, 255, 128, 128, 128,
      255, 128, 128, 128, 255, 255, 0,   0,   255, 255, 0,   0,   255, 255, 0,
      0,   255, 255, 0,   0,   255, 0,   255, 0,   255, 0,   255, 0,   255, 0,
      255, 0,   255, 0,   255, 0,   255, 0,   0,   255, 128, 0,   0,   255, 128,
      0,   0,   255, 128, 0,   0,   255, 128};

  const size_t pixels = CalculateRGBAPixels(sizeof(canvas));
  const size_t rgb_size = CalculateRGBOutputSize(sizeof(canvas));
  unsigned char output[rgb_size];
  unsigned char expected[rgb_size];
  memset(output, 0, rgb_size);
  memset(expected, 0, rgb_size);

  blink::RGBAtoRGBScalar(canvas, static_cast<unsigned>(pixels), expected);
  blink::RGBAtoRGBNeon(canvas, static_cast<unsigned>(pixels), output);

  EXPECT_EQ(memcmp(expected, output, rgb_size), 0);
}

static base::TimeDelta TestNpixels(bool fast_path = true,
                                   const size_t width = 1024,
                                   const size_t height = 1024,
                                   bool set_alpha = true) {
  const size_t pixels = width * height;
  const size_t canvas_len = kChannelsRGBA * width * height;
  const size_t output_len = kChannelsRGB * width * height;
  unsigned char* canvas = new unsigned char[canvas_len];
  unsigned char* output = new unsigned char[output_len];

  auto cleanup = [&]() {
    if (canvas)
      delete[] canvas;
    if (output)
      delete[] output;
  };

  if (!canvas || !output) {
    cleanup();
    return base::TimeDelta();
  }

  if (set_alpha) {
    memset(canvas, 128, canvas_len);
  } else {
    memset(canvas, 200, canvas_len);
  }

  base::ElapsedTimer run_time;
  if (fast_path) {
    blink::RGBAtoRGBNeon(canvas, static_cast<unsigned>(pixels), output);
  } else {
    blink::RGBAtoRGBScalar(canvas, static_cast<unsigned>(pixels), output);
  }

  auto result = run_time.Elapsed();
  cleanup();
  return result;
}

TEST_F(RGBAtoRGBTest, testPerf1k) {
  auto neon_elapsed = TestNpixels();
  auto scalar_elapsed = TestNpixels(false);

  EXPECT_TRUE(neon_elapsed < scalar_elapsed)
      << "Neon: " << neon_elapsed << "\tScalar: " << scalar_elapsed
      << std::endl;
}

TEST_F(RGBAtoRGBTest, testPerf4k) {
  auto neon_elapsed = TestNpixels(true, 4000, 4000);
  auto scalar_elapsed = TestNpixels(false, 4000, 4000);

  EXPECT_TRUE(neon_elapsed < scalar_elapsed)
      << "Neon: " << neon_elapsed << "\tScalar: " << scalar_elapsed
      << std::endl;
}

// This width will force the tail case, cause width = (16 * 64) + 15.
static bool TestRandNpixels(const size_t width = 1039,
                            const size_t height = 1024,
                            bool set_alpha = true) {
  const size_t pixels = width * height;
  const size_t canvas_len = kChannelsRGBA * pixels;
  const size_t output_len = kChannelsRGB * pixels;
  unsigned char* canvas = new unsigned char[canvas_len];
  unsigned char* expected = new unsigned char[output_len];
  unsigned char* output = new unsigned char[output_len];

  auto cleanup = [&]() {
    if (canvas)
      delete[] canvas;
    if (expected)
      delete[] expected;
    if (output)
      delete[] output;
  };

  if (!canvas || !output || !expected) {
    cleanup();
    return false;
  }

  if (set_alpha) {
    memset(canvas, 128, canvas_len);
  } else {
    memset(canvas, 200, canvas_len);
  }

  srand(time(0));
  unsigned char* ptr = canvas;
  for (size_t i = 0; i < pixels; ++i) {
    *ptr++ = static_cast<unsigned char>(rand() % 255);
    *ptr++ = static_cast<unsigned char>(rand() % 255);
    *ptr++ = static_cast<unsigned char>(rand() % 255);
    *ptr++ = static_cast<unsigned char>(rand() % 255);
  }

  blink::RGBAtoRGBScalar(canvas, static_cast<unsigned>(pixels), expected);
  blink::RGBAtoRGBNeon(canvas, static_cast<unsigned>(pixels), output);

  bool result = memcmp(expected, output, output_len) == 0;

  cleanup();
  return result;
}

TEST_F(RGBAtoRGBTest, randomPixels) {
  EXPECT_TRUE(TestRandNpixels());
}

#endif
}  // namespace blink
