// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "remoting/base/util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkSize.h"

static const int kWidth = 32 ;
static const int kHeight = 24 ;
static const int kBytesPerPixel = 4;
static const int kYStride = kWidth;
static const int kUvStride = kWidth / 2;
static const int kRgbStride = kWidth * kBytesPerPixel;
static const uint32 kFillColor = 0xffffff;

namespace remoting {

class YuvToRgbTester {
 public:
  YuvToRgbTester() {
    yuv_buffer_size_ = (kYStride + kUvStride) * kHeight;
    yuv_buffer_.reset(new uint8[yuv_buffer_size_]);
    yplane_ = yuv_buffer_.get();
    uplane_ = yplane_ + (kYStride * kHeight);
    vplane_ = uplane_ + (kUvStride * kHeight / 2);

    rgb_buffer_size_ = kWidth * kHeight * kBytesPerPixel;
    rgb_buffer_.reset(new uint8[rgb_buffer_size_]);

    ResetYuvBuffer();
    ResetRgbBuffer();
  }

  ~YuvToRgbTester() {}

  void ResetYuvBuffer() {
    memset(yuv_buffer_.get(), 0, yuv_buffer_size_);
  }

  void ResetRgbBuffer() {
    memset(rgb_buffer_.get(), 0, rgb_buffer_size_);
  }

  void FillRgbBuffer(const SkIRect& rect) {
    uint32* ptr = reinterpret_cast<uint32*>(
        rgb_buffer_.get() + (rect.top() * kRgbStride) +
        (rect.left() * kBytesPerPixel));
    int width = rect.width();
    for (int height = rect.height(); height > 0; --height) {
      std::fill(ptr, ptr + width, kFillColor);
      ptr += kRgbStride / kBytesPerPixel;
    }
  }

  // Check the the desination buffer is filled within expected bounds.
  void  CheckRgbBuffer(const SkIRect& rect) {
    uint32* ptr = reinterpret_cast<uint32*>(rgb_buffer_.get());
    for (int y = 0; y < kHeight; ++y) {
      if (y < rect.top() || rect.bottom() <= y) {
        // The whole line should be intact.
        EXPECT_EQ((ptrdiff_t)kWidth,
                  std::count(ptr, ptr + kWidth, 0u));
      } else {
        // The space before the painted rectangle should be intact.
        EXPECT_EQ((ptrdiff_t)rect.left(),
                  std::count(ptr, ptr + rect.left(), 0u));

        // All pixels of the target rectangle should be touched.
        EXPECT_EQ(ptr + rect.right(),
                  std::find(ptr + rect.left(), ptr + rect.right(), 0u));

        // The space after the painted rectangle should be intact.
        EXPECT_EQ((ptrdiff_t)kWidth - rect.right(),
                  std::count(ptr + rect.right(), ptr + kWidth, 0u));
      }
      ptr += kRgbStride / kBytesPerPixel;
    }
  }

  void RunTest(const SkISize dest_size, const SkIRect& rect) {
    ASSERT_TRUE(SkIRect::MakeSize(dest_size).contains(rect));

    // Reset buffers.
    ResetYuvBuffer();
    ResetRgbBuffer();
    FillRgbBuffer(rect);

    // RGB -> YUV
    ConvertRGB32ToYUVWithRect(rgb_buffer_.get(),
                              yplane_,
                              uplane_,
                              vplane_,
                              0,
                              0,
                              kWidth,
                              kHeight,
                              kRgbStride,
                              kYStride,
                              kUvStride);

    // Reset RGB buffer and do opposite conversion.
    ResetRgbBuffer();
    ConvertAndScaleYUVToRGB32Rect(yplane_,
                                  uplane_,
                                  vplane_,
                                  kYStride,
                                  kUvStride,
                                  SkISize::Make(kWidth, kHeight),
                                  SkIRect::MakeWH(kWidth, kHeight),
                                  rgb_buffer_.get(),
                                  kRgbStride,
                                  dest_size,
                                  SkIRect::MakeSize(dest_size),
                                  rect);

    // Check if it worked out.
    CheckRgbBuffer(rect);
  }

  void TestBasicConversion() {
    // Whole buffer.
    RunTest(SkISize::Make(kWidth, kHeight), SkIRect::MakeWH(kWidth, kHeight));
  }

 private:
  size_t yuv_buffer_size_;
  scoped_array<uint8> yuv_buffer_;
  uint8* yplane_;
  uint8* uplane_;
  uint8* vplane_;

  size_t rgb_buffer_size_;
  scoped_array<uint8> rgb_buffer_;

  DISALLOW_COPY_AND_ASSIGN(YuvToRgbTester);
};

TEST(YuvToRgbTest, BasicConversion) {
  YuvToRgbTester tester;
  tester.TestBasicConversion();
}

TEST(YuvToRgbTest, Clipping) {
  YuvToRgbTester tester;

  SkISize dest_size = SkISize::Make(kWidth, kHeight);
  SkIRect rect = SkIRect::MakeLTRB(0, 0, kWidth - 1, kHeight - 1);
  for (int i = 0; i < 16; ++i) {
    SkIRect dest_rect = rect;
    if ((i & 1) != 0)
      dest_rect.fLeft += 1;
    if ((i & 2) != 0)
      dest_rect.fTop += 1;
    if ((i & 4) != 0)
      dest_rect.fRight += 1;
    if ((i & 8) != 0)
      dest_rect.fBottom += 1;

    tester.RunTest(dest_size, dest_rect);
  }
}

TEST(YuvToRgbTest, ClippingAndScaling) {
  YuvToRgbTester tester;

  SkISize dest_size = SkISize::Make(kWidth - 10, kHeight - 10);
  SkIRect rect = SkIRect::MakeLTRB(5, 5, kWidth - 11, kHeight - 11);
  for (int i = 0; i < 16; ++i) {
    SkIRect dest_rect = rect;
    if ((i & 1) != 0)
      dest_rect.fLeft += 1;
    if ((i & 2) != 0)
      dest_rect.fTop += 1;
    if ((i & 4) != 0)
      dest_rect.fRight += 1;
    if ((i & 8) != 0)
      dest_rect.fBottom += 1;

    tester.RunTest(dest_size, dest_rect);
  }
}

}  // namespace remoting
