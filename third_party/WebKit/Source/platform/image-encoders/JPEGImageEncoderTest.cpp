// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/timer/elapsed_timer.h"
#include "platform/image-encoders/RGBAtoRGB.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/build_config.h"

namespace blink {

class RGBAtoRGBTest : public ::testing::Test {
 public:
  RGBAtoRGBTest() {}
};

static const size_t channelsRGBA = 4;
static const size_t channelsRGB = 3;

inline size_t calculateRGBAPixels(size_t inputBufferSize) {
  size_t pixels = inputBufferSize / channelsRGBA;
  return pixels;
}

inline size_t calculateRGBOutputSize(size_t inputBufferSize) {
  size_t pixels = calculateRGBAPixels(inputBufferSize);
  pixels *= channelsRGB;
  return pixels;
}

TEST_F(RGBAtoRGBTest, testOpaqueCaseEven8pixels) {
  unsigned char canvas[] = {255, 0,   0, 255, 255, 0,   0,   255, 255, 0,  0,
                            255, 255, 0, 0,   255, 0,   255, 0,   255, 0,  255,
                            0,   255, 0, 255, 0,   255, 0,   255, 0,   255};

  unsigned char expected[] = {255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 0,
                              255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0};
#if OS(WIN)
  // Windows release bot can't be reasoned with (compiler error C2131).
  static const constexpr size_t pixels = sizeof(canvas) / channelsRGBA;
  static const constexpr size_t rgbSize = pixels * channelsRGB;
#else
  const size_t pixels = calculateRGBAPixels(sizeof(canvas));
  const size_t rgbSize = calculateRGBOutputSize(sizeof(canvas));
#endif

  unsigned char output[rgbSize];
  memset(output, 0, rgbSize);

  blink::RGBAtoRGB(canvas, static_cast<unsigned>(pixels), output);

  EXPECT_EQ(memcmp(expected, output, rgbSize), 0);
}

#ifdef __ARM_NEON__
TEST_F(RGBAtoRGBTest, testCaseEven16pixels) {
  unsigned char canvas[] = {
      255, 0,   0,   255, 255, 0,   0,   255, 255, 0,   0,   255, 255,
      0,   0,   255, 0,   255, 0,   255, 0,   255, 0,   255, 0,   255,
      0,   255, 0,   255, 0,   255, 0,   0,   255, 128, 0,   0,   255,
      128, 0,   0,   255, 128, 0,   0,   255, 128, 128, 128, 128, 128,
      128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128};

  const size_t pixels = calculateRGBAPixels(sizeof(canvas));
  const size_t rgbSize = calculateRGBOutputSize(sizeof(canvas));
  unsigned char output[rgbSize];
  unsigned char expected[rgbSize];
  memset(output, 0, rgbSize);
  memset(expected, 0, rgbSize);

  blink::RGBAtoRGBScalar(canvas, static_cast<unsigned>(pixels), expected);
  blink::RGBAtoRGBNeon(canvas, static_cast<unsigned>(pixels), output);

  EXPECT_EQ(memcmp(expected, output, rgbSize), 0);
}

TEST_F(RGBAtoRGBTest, testCaseOdd17pixels) {
  unsigned char canvas[] = {
      255, 0,   0,   255, 255, 0,   0,   255, 255, 0,   0,   255, 255, 0,
      0,   255, 0,   255, 0,   255, 0,   255, 0,   255, 0,   255, 0,   255,
      0,   255, 0,   255, 0,   0,   255, 128, 0,   0,   255, 128, 0,   0,
      255, 128, 0,   0,   255, 128, 128, 128, 128, 128, 128, 128, 128, 128,
      128, 128, 128, 128, 128, 128, 128, 128, 10,  10,  10,  100};

  const size_t pixels = calculateRGBAPixels(sizeof(canvas));
  const size_t rgbSize = calculateRGBOutputSize(sizeof(canvas));
  unsigned char output[rgbSize];
  unsigned char expected[rgbSize];
  memset(output, 0, rgbSize);
  memset(expected, 0, rgbSize);

  blink::RGBAtoRGBScalar(canvas, static_cast<unsigned>(pixels), expected);
  blink::RGBAtoRGBNeon(canvas, static_cast<unsigned>(pixels), output);

  EXPECT_EQ(memcmp(expected, output, rgbSize), 0);
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

  const size_t pixels = calculateRGBAPixels(sizeof(canvas));
  const size_t rgbSize = calculateRGBOutputSize(sizeof(canvas));
  unsigned char output[rgbSize];
  unsigned char expected[rgbSize];
  memset(output, 0, rgbSize);
  memset(expected, 0, rgbSize);

  blink::RGBAtoRGBScalar(canvas, static_cast<unsigned>(pixels), expected);
  blink::RGBAtoRGBNeon(canvas, static_cast<unsigned>(pixels), output);

  EXPECT_EQ(memcmp(expected, output, rgbSize), 0);
}

static base::TimeDelta testNpixels(bool fastPath = true,
                                   const size_t width = 1024,
                                   const size_t height = 1024,
                                   bool setAlpha = true) {
  const size_t pixels = width * height;
  const size_t canvasLen = channelsRGBA * width * height;
  const size_t outputLen = channelsRGB * width * height;
  unsigned char* canvas = new unsigned char[canvasLen];
  unsigned char* output = new unsigned char[outputLen];

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

  if (setAlpha) {
    memset(canvas, 128, canvasLen);
  } else {
    memset(canvas, 200, canvasLen);
  }

  base::ElapsedTimer runTime;
  if (fastPath) {
    blink::RGBAtoRGBNeon(canvas, static_cast<unsigned>(pixels), output);
  } else {
    blink::RGBAtoRGBScalar(canvas, static_cast<unsigned>(pixels), output);
  }

  auto result = runTime.Elapsed();
  cleanup();
  return result;
}

TEST_F(RGBAtoRGBTest, testPerf1k) {
  auto neonElapsed = testNpixels();
  auto scalarElapsed = testNpixels(false);

  EXPECT_TRUE(neonElapsed < scalarElapsed)
      << "Neon: " << neonElapsed << "\tScalar: " << scalarElapsed << std::endl;
}

TEST_F(RGBAtoRGBTest, testPerf4k) {
  auto neonElapsed = testNpixels(true, 4000, 4000);
  auto scalarElapsed = testNpixels(false, 4000, 4000);

  EXPECT_TRUE(neonElapsed < scalarElapsed)
      << "Neon: " << neonElapsed << "\tScalar: " << scalarElapsed << std::endl;
}

// This width will force the tail case, cause width = (16 * 64) + 15.
static bool testRandNpixels(const size_t width = 1039,
                            const size_t height = 1024,
                            bool setAlpha = true) {
  const size_t pixels = width * height;
  const size_t canvasLen = channelsRGBA * pixels;
  const size_t outputLen = channelsRGB * pixels;
  unsigned char* canvas = new unsigned char[canvasLen];
  unsigned char* expected = new unsigned char[outputLen];
  unsigned char* output = new unsigned char[outputLen];

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

  if (setAlpha) {
    memset(canvas, 128, canvasLen);
  } else {
    memset(canvas, 200, canvasLen);
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

  bool result = memcmp(expected, output, outputLen) == 0;

  cleanup();
  return result;
}

TEST_F(RGBAtoRGBTest, randomPixels) {
  EXPECT_TRUE(testRandNpixels());
}

#endif
}  // namespace blink
