// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "remoting/base/util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv/convert_from_argb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

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

  void FillRgbBuffer(const webrtc::DesktopRect& rect) {
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
  void  CheckRgbBuffer(const webrtc::DesktopRect& rect) {
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

  void RunTest(const webrtc::DesktopSize dest_size,
               const webrtc::DesktopRect& rect) {
    ASSERT_TRUE(
        DoesRectContain(webrtc::DesktopRect::MakeSize(dest_size), rect));

    // Reset buffers.
    ResetYuvBuffer();
    ResetRgbBuffer();
    FillRgbBuffer(rect);

    // RGB -> YUV
    libyuv::ARGBToI420(rgb_buffer_.get(),
                       kRgbStride,
                       yplane_,
                       kYStride,
                       uplane_,
                       kUvStride,
                       vplane_,
                       kUvStride,
                       kWidth,
                       kHeight);

    // Reset RGB buffer and do opposite conversion.
    ResetRgbBuffer();
    ConvertAndScaleYUVToRGB32Rect(yplane_,
                                  uplane_,
                                  vplane_,
                                  kYStride,
                                  kUvStride,
                                  webrtc::DesktopSize(kWidth, kHeight),
                                  webrtc::DesktopRect::MakeWH(kWidth, kHeight),
                                  rgb_buffer_.get(),
                                  kRgbStride,
                                  dest_size,
                                  webrtc::DesktopRect::MakeSize(dest_size),
                                  rect);

    // Check if it worked out.
    CheckRgbBuffer(rect);
  }

  void TestBasicConversion() {
    // Whole buffer.
    RunTest(webrtc::DesktopSize(kWidth, kHeight),
            webrtc::DesktopRect::MakeWH(kWidth, kHeight));
  }

 private:
  size_t yuv_buffer_size_;
  scoped_ptr<uint8[]> yuv_buffer_;
  uint8* yplane_;
  uint8* uplane_;
  uint8* vplane_;

  size_t rgb_buffer_size_;
  scoped_ptr<uint8[]> rgb_buffer_;

  DISALLOW_COPY_AND_ASSIGN(YuvToRgbTester);
};

TEST(YuvToRgbTest, BasicConversion) {
  YuvToRgbTester tester;
  tester.TestBasicConversion();
}

TEST(YuvToRgbTest, Clipping) {
  YuvToRgbTester tester;

  webrtc::DesktopSize dest_size = webrtc::DesktopSize(kWidth, kHeight);
  webrtc::DesktopRect rect =
      webrtc::DesktopRect::MakeLTRB(0, 0, kWidth - 1, kHeight - 1);
  // TODO(fbarchard): Allow top/left clipping to odd boundary.
  for (int i = 0; i < 16; ++i) {
    webrtc::DesktopRect dest_rect = webrtc::DesktopRect::MakeLTRB(
        rect.left() + ((i & 1) ? 2 : 0),
        rect.top() + ((i & 2) ? 2 : 0),
        rect.right() - ((i & 4) ? 1 : 0),
        rect.bottom() - ((i & 8) ? 1 : 0));

    tester.RunTest(dest_size, dest_rect);
  }
}

TEST(YuvToRgbTest, ClippingAndScaling) {
  YuvToRgbTester tester;

  webrtc::DesktopSize dest_size =
      webrtc::DesktopSize(kWidth - 10, kHeight - 10);
  webrtc::DesktopRect rect =
      webrtc::DesktopRect::MakeLTRB(6, 6, kWidth - 11, kHeight - 11);
  for (int i = 0; i < 16; ++i) {
    webrtc::DesktopRect dest_rect = webrtc::DesktopRect::MakeLTRB(
        rect.left() + ((i & 1) ? 2 : 0),
        rect.top() + ((i & 2) ? 2 : 0),
        rect.right() - ((i & 4) ? 1 : 0),
        rect.bottom() - ((i & 8) ? 1 : 0));

    tester.RunTest(dest_size, dest_rect);
  }
}

TEST(ReplaceLfByCrLfTest, Basic) {
  EXPECT_EQ("ab", ReplaceLfByCrLf("ab"));
  EXPECT_EQ("\r\nab", ReplaceLfByCrLf("\nab"));
  EXPECT_EQ("\r\nab\r\n", ReplaceLfByCrLf("\nab\n"));
  EXPECT_EQ("\r\nab\r\ncd", ReplaceLfByCrLf("\nab\ncd"));
  EXPECT_EQ("\r\nab\r\ncd\r\n", ReplaceLfByCrLf("\nab\ncd\n"));
  EXPECT_EQ("\r\n\r\nab\r\n\r\ncd\r\n\r\n",
      ReplaceLfByCrLf("\n\nab\n\ncd\n\n"));
}

TEST(ReplaceLfByCrLfTest, Speed) {
  int kLineSize = 128;
  std::string line(kLineSize, 'a');
  line[kLineSize - 1] = '\n';
  // Make a 10M string.
  int kLineNum = 10 * 1024 * 1024 / kLineSize;
  std::string buffer;
  buffer.resize(kLineNum * kLineSize);
  for (int i = 0; i < kLineNum; ++i) {
    memcpy(&buffer[i * kLineSize], &line[0], kLineSize);
  }
  // Convert the string.
  buffer = ReplaceLfByCrLf(buffer);
  // Check the converted string.
  EXPECT_EQ(static_cast<size_t>((kLineSize + 1) * kLineNum), buffer.size());
  const char* p = &buffer[0];
  for (int i = 0; i < kLineNum; ++i) {
    EXPECT_EQ(0, memcmp(&line[0], p, kLineSize - 1));
    p += kLineSize - 1;
    EXPECT_EQ('\r', *p++);
    EXPECT_EQ('\n', *p++);
  }
}

TEST(ReplaceCrLfByLfTest, Basic) {
  EXPECT_EQ("ab", ReplaceCrLfByLf("ab"));
  EXPECT_EQ("\nab", ReplaceCrLfByLf("\r\nab"));
  EXPECT_EQ("\nab\n", ReplaceCrLfByLf("\r\nab\r\n"));
  EXPECT_EQ("\nab\ncd", ReplaceCrLfByLf("\r\nab\r\ncd"));
  EXPECT_EQ("\nab\ncd\n", ReplaceCrLfByLf("\r\nab\r\ncd\n"));
  EXPECT_EQ("\n\nab\n\ncd\n\n",
      ReplaceCrLfByLf("\r\n\r\nab\r\n\r\ncd\r\n\r\n"));
  EXPECT_EQ("\rab\rcd\r", ReplaceCrLfByLf("\rab\rcd\r"));
}

TEST(ReplaceCrLfByLfTest, Speed) {
  int kLineSize = 128;
  std::string line(kLineSize, 'a');
  line[kLineSize - 2] = '\r';
  line[kLineSize - 1] = '\n';
  // Make a 10M string.
  int kLineNum = 10 * 1024 * 1024 / kLineSize;
  std::string buffer;
  buffer.resize(kLineNum * kLineSize);
  for (int i = 0; i < kLineNum; ++i) {
    memcpy(&buffer[i * kLineSize], &line[0], kLineSize);
  }
  // Convert the string.
  buffer = ReplaceCrLfByLf(buffer);
  // Check the converted string.
  EXPECT_EQ(static_cast<size_t>((kLineSize - 1) * kLineNum), buffer.size());
  const char* p = &buffer[0];
  for (int i = 0; i < kLineNum; ++i) {
    EXPECT_EQ(0, memcmp(&line[0], p, kLineSize - 2));
    p += kLineSize - 2;
    EXPECT_EQ('\n', *p++);
  }
}

TEST(StringIsUtf8Test, Basic) {
  EXPECT_TRUE(StringIsUtf8("", 0));
  EXPECT_TRUE(StringIsUtf8("\0", 1));
  EXPECT_TRUE(StringIsUtf8("abc", 3));
  EXPECT_TRUE(StringIsUtf8("\xc0\x80", 2));
  EXPECT_TRUE(StringIsUtf8("\xe0\x80\x80", 3));
  EXPECT_TRUE(StringIsUtf8("\xf0\x80\x80\x80", 4));
  EXPECT_TRUE(StringIsUtf8("\xf8\x80\x80\x80\x80", 5));
  EXPECT_TRUE(StringIsUtf8("\xfc\x80\x80\x80\x80\x80", 6));

  // Not enough continuation characters
  EXPECT_FALSE(StringIsUtf8("\xc0", 1));
  EXPECT_FALSE(StringIsUtf8("\xe0\x80", 2));
  EXPECT_FALSE(StringIsUtf8("\xf0\x80\x80", 3));
  EXPECT_FALSE(StringIsUtf8("\xf8\x80\x80\x80", 4));
  EXPECT_FALSE(StringIsUtf8("\xfc\x80\x80\x80\x80", 5));

  // One more continuation character than needed
  EXPECT_FALSE(StringIsUtf8("\xc0\x80\x80", 3));
  EXPECT_FALSE(StringIsUtf8("\xe0\x80\x80\x80", 4));
  EXPECT_FALSE(StringIsUtf8("\xf0\x80\x80\x80\x80", 5));
  EXPECT_FALSE(StringIsUtf8("\xf8\x80\x80\x80\x80\x80", 6));
  EXPECT_FALSE(StringIsUtf8("\xfc\x80\x80\x80\x80\x80\x80", 7));

  // Invalid first byte
  EXPECT_FALSE(StringIsUtf8("\xfe\x80\x80\x80\x80\x80\x80", 7));
  EXPECT_FALSE(StringIsUtf8("\xff\x80\x80\x80\x80\x80\x80", 7));

  // Invalid continuation byte
  EXPECT_FALSE(StringIsUtf8("\xc0\x00", 2));
  EXPECT_FALSE(StringIsUtf8("\xc0\x40", 2));
  EXPECT_FALSE(StringIsUtf8("\xc0\xc0", 2));
}

}  // namespace remoting
