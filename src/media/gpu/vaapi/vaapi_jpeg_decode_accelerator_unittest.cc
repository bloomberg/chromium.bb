// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <string.h>

#include <string>

// This has to be included first.
// See http://code.google.com/p/googletest/issues/detail?id=371
#include "testing/gtest/include/gtest/gtest.h"

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "media/base/test_data_util.h"
#include "media/base/video_frame.h"
#include "media/filters/jpeg_parser.h"
#include "media/gpu/vaapi/vaapi_jpeg_decode_accelerator.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "mojo/core/embedder/embedder.h"

namespace media {
namespace {

constexpr const char* kTestFilename = "pixel-1280x720.jpg";
constexpr const char* kExpectedMd5Sum = "6e9e1716073c9a9a1282e3f0e0dab743";

void LogOnError() {
  LOG(FATAL) << "Oh noes! Decoder failed";
}

// Find the location of the specified test file. If a file with specified path
// is not found, treat the path as being relative to the standard test file
// directory.
base::FilePath FindTestDataFilePath(const std::string& file_name) {
  base::FilePath file_path = base::FilePath(file_name);
  return PathExists(file_path) ? file_path : GetTestDataFilePath(file_name);
}

}  // namespace

class VaapiJpegDecodeAcceleratorTest : public ::testing::Test {
 protected:
  VaapiJpegDecodeAcceleratorTest() {}

  void SetUp() override {
    base::RepeatingClosure report_error_cb = base::BindRepeating(&LogOnError);
    wrapper_ = VaapiWrapper::Create(VaapiWrapper::kDecode,
                                    VAProfileJPEGBaseline, report_error_cb);
    ASSERT_TRUE(wrapper_);

    base::FilePath input_file = FindTestDataFilePath(kTestFilename);

    ASSERT_TRUE(base::ReadFileToString(input_file, &jpeg_data_))
        << "failed to read input data from " << input_file.value();
  }

  void TearDown() override { wrapper_ = nullptr; }

  bool VerifyDecode(const JpegParseResult& parse_result,
                    const std::string& md5sum);
  bool Decode(VaapiWrapper* vaapi_wrapper,
              const JpegParseResult& parse_result,
              VASurfaceID va_surface);

 protected:
  scoped_refptr<VaapiWrapper> wrapper_;
  std::string jpeg_data_;
};

bool VaapiJpegDecodeAcceleratorTest::VerifyDecode(
    const JpegParseResult& parse_result,
    const std::string& expected_md5sum) {
  gfx::Size size(parse_result.frame_header.coded_width,
                 parse_result.frame_header.coded_height);

  std::vector<VASurfaceID> va_surfaces;
  if (!wrapper_->CreateSurfaces(VA_RT_FORMAT_YUV420, size, 1, &va_surfaces))
    return false;

  if (!Decode(wrapper_.get(), parse_result, va_surfaces[0])) {
    LOG(ERROR) << "Decode failed";
    return false;
  }

  VAImage image;
  VAImageFormat format;
  const uint32_t kI420Fourcc = VA_FOURCC('I', '4', '2', '0');
  memset(&image, 0, sizeof(image));
  memset(&format, 0, sizeof(format));
  format.fourcc = kI420Fourcc;
  format.byte_order = VA_LSB_FIRST;
  format.bits_per_pixel = 12;  // 12 for I420

  void* mem;
  if (!wrapper_->GetVaImage(va_surfaces[0], &format, size, &image, &mem)) {
    LOG(ERROR) << "Cannot get VAImage";
    return false;
  }
  EXPECT_EQ(kI420Fourcc, image.format.fourcc);

  base::StringPiece result(reinterpret_cast<const char*>(mem),
                           VideoFrame::AllocationSize(PIXEL_FORMAT_I420, size));
  EXPECT_EQ(expected_md5sum, base::MD5String(result));

  wrapper_->ReturnVaImage(&image);

  return true;
}

bool VaapiJpegDecodeAcceleratorTest::Decode(VaapiWrapper* vaapi_wrapper,
                                            const JpegParseResult& parse_result,
                                            VASurfaceID va_surface) {
  return VaapiJpegDecodeAccelerator::DoDecode(vaapi_wrapper, parse_result,
                                              va_surface);
}

TEST_F(VaapiJpegDecodeAcceleratorTest, DecodeSuccess) {
  JpegParseResult parse_result;
  ASSERT_TRUE(
      ParseJpegPicture(reinterpret_cast<const uint8_t*>(jpeg_data_.data()),
                       jpeg_data_.size(), &parse_result));

  EXPECT_TRUE(VerifyDecode(parse_result, kExpectedMd5Sum));
}

TEST_F(VaapiJpegDecodeAcceleratorTest, DecodeFail) {
  JpegParseResult parse_result;
  ASSERT_TRUE(
      ParseJpegPicture(reinterpret_cast<const uint8_t*>(jpeg_data_.data()),
                       jpeg_data_.size(), &parse_result));

  // Not supported by VAAPI.
  parse_result.frame_header.num_components = 1;
  parse_result.scan.num_components = 1;

  gfx::Size size(parse_result.frame_header.coded_width,
                 parse_result.frame_header.coded_height);

  std::vector<VASurfaceID> va_surfaces;
  ASSERT_TRUE(
      wrapper_->CreateSurfaces(VA_RT_FORMAT_YUV420, size, 1, &va_surfaces));

  EXPECT_FALSE(Decode(wrapper_.get(), parse_result, va_surfaces[0]));
}

}  // namespace media
