// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include <va/va.h>

// This has to be included first.
// See http://code.google.com/p/googletest/issues/detail?id=371
#include "testing/gtest/include/gtest/gtest.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/synchronization/lock.h"
#include "base/test/gtest_util.h"
#include "base/thread_annotations.h"
#include "media/base/test_data_util.h"
#include "media/base/video_types.h"
#include "media/filters/jpeg_parser.h"
#include "media/gpu/vaapi/vaapi_jpeg_decoder.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/geometry/size.h"

namespace media {
namespace {

constexpr const char* kYuv422Filename = "pixel-1280x720.jpg";
constexpr const char* kYuv420Filename = "pixel-1280x720-yuv420.jpg";

struct TestParam {
  const char* filename;
};

const TestParam kTestCases[] = {
    {kYuv422Filename},
    {kYuv420Filename},
};

// Any number above 99.5% should do, experimentally we like a wee higher.
constexpr double kMinSsim = 0.997;

// This file is not supported by the VAAPI, so we don't define expectations on
// the decode result.
constexpr const char* kUnsupportedFilename = "pixel-1280x720-grayscale.jpg";

constexpr VAImageFormat kImageFormatI420 = {
    .fourcc = VA_FOURCC_I420,
    .byte_order = VA_LSB_FIRST,
    .bits_per_pixel = 12,
};

// Compares the result of sw decoding |encoded_image| with |decoded_image| using
// SSIM. Returns true if all conversions work and SSIM is above a given
// threshold (kMinSsim), or false otherwise.
bool CompareImages(base::span<const uint8_t> encoded_image,
                   const ScopedVAImage* decoded_image) {
  JpegParseResult parse_result;
  const bool result = ParseJpegPicture(encoded_image.data(),
                                       encoded_image.size(), &parse_result);
  if (!result)
    return false;

  const uint16_t width = parse_result.frame_header.visible_width;
  const uint16_t height = parse_result.frame_header.visible_height;

  if (width != decoded_image->image()->width ||
      height != decoded_image->image()->height) {
    DLOG(ERROR) << "Wrong expected decoded JPEG size, " << width << "x"
                << height << " versus VaAPI provided "
                << decoded_image->image()->width << "x"
                << decoded_image->image()->height;
    return false;
  }
  const uint16_t even_width = (width + 1) / 2;
  const uint16_t even_height = (height + 1) / 2;

  auto ref_y = std::make_unique<uint8_t[]>(width * height);
  auto ref_u = std::make_unique<uint8_t[]>(even_width * even_height);
  auto ref_v = std::make_unique<uint8_t[]>(even_width * even_height);

  const int conversion_result = libyuv::ConvertToI420(
      encoded_image.data(), encoded_image.size(), ref_y.get(), width,
      ref_u.get(), even_width, ref_v.get(), even_width, 0, 0, width, height,
      width, height, libyuv::kRotate0, libyuv::FOURCC_MJPG);
  if (conversion_result != 0) {
    DLOG(ERROR) << "libyuv conversion error";
    return false;
  }

  const uint32_t va_fourcc = decoded_image->image()->format.fourcc;
  if (!(va_fourcc == VA_FOURCC_I420 || va_fourcc == VA_FOURCC_YUY2 ||
        va_fourcc == VA_FOURCC('Y', 'U', 'Y', 'V'))) {
    DLOG(ERROR) << "Not supported FourCC: " << FourccToString(va_fourcc);
    return false;
  }
  const uint32_t libyuv_fourcc =
      (va_fourcc == VA_FOURCC_I420) ? libyuv::FOURCC_I420 : libyuv::FOURCC_YUY2;

  if (libyuv_fourcc == libyuv::FOURCC_I420) {
    const auto* decoded_data_y =
        static_cast<const uint8_t*>(decoded_image->va_buffer()->data()) +
        decoded_image->image()->offsets[0];
    const auto* decoded_data_u =
        static_cast<const uint8_t*>(decoded_image->va_buffer()->data()) +
        decoded_image->image()->offsets[1];
    const auto* decoded_data_v =
        static_cast<const uint8_t*>(decoded_image->va_buffer()->data()) +
        decoded_image->image()->offsets[2];

    const double ssim = libyuv::I420Ssim(
        ref_y.get(), width, ref_u.get(), even_width, ref_v.get(), even_width,
        decoded_data_y,
        base::checked_cast<int>(decoded_image->image()->pitches[0]),
        decoded_data_u,
        base::checked_cast<int>(decoded_image->image()->pitches[1]),
        decoded_data_v,
        base::checked_cast<int>(decoded_image->image()->pitches[2]), width,
        height);
    if (ssim < kMinSsim) {
      DLOG(ERROR) << "Too low SSIM: " << ssim << " < " << kMinSsim;
      return false;
    }
  } else {
    auto temp_y = std::make_unique<uint8_t[]>(width * height);
    auto temp_u = std::make_unique<uint8_t[]>(even_width * even_height);
    auto temp_v = std::make_unique<uint8_t[]>(even_width * even_height);

    // TODO(crbug.com/868400): support other formats/planarities/pitches.
    constexpr uint32_t kNumPlanesYuv422 = 1u;
    constexpr uint32_t kBytesPerPixelYuv422 = 2u;
    if (decoded_image->image()->num_planes != kNumPlanesYuv422 ||
        decoded_image->image()->pitches[0] != (width * kBytesPerPixelYuv422)) {
      DLOG(ERROR) << "Too many planes (got "
                  << decoded_image->image()->num_planes << ", expected "
                  << kNumPlanesYuv422 << ") or rows not tightly packed (got "
                  << decoded_image->image()->pitches[0] << ", expected "
                  << (width * kBytesPerPixelYuv422) << "), aborting test";
      return false;
    }

    const int conversion_result = libyuv::ConvertToI420(
        static_cast<const uint8_t*>(decoded_image->va_buffer()->data()),
        base::strict_cast<size_t>(decoded_image->image()->data_size),
        temp_y.get(), width, temp_u.get(), even_width, temp_v.get(), even_width,
        0, 0, width, height, width, height, libyuv::kRotate0, libyuv_fourcc);
    if (conversion_result != 0) {
      DLOG(ERROR) << "libyuv conversion error";
      return false;
    }

    const double ssim = libyuv::I420Ssim(
        ref_y.get(), width, ref_u.get(), even_width, ref_v.get(), even_width,
        temp_y.get(), width, temp_u.get(), even_width, temp_v.get(), even_width,
        width, height);
    if (ssim < kMinSsim) {
      DLOG(ERROR) << "Too low SSIM: " << ssim << " < " << kMinSsim;
      return false;
    }
  }
  return true;
}

}  // namespace

class VaapiJpegDecoderTest : public testing::TestWithParam<TestParam> {
 protected:
  VaapiJpegDecoderTest() {
    const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
    if (cmd_line && cmd_line->HasSwitch("test_data_path"))
      test_data_path_ = cmd_line->GetSwitchValueASCII("test_data_path");
  }

  void SetUp() override {
    ASSERT_TRUE(decoder_.Initialize(base::BindRepeating(
        []() { LOG(FATAL) << "Oh noes! Decoder failed"; })));
  }

  base::FilePath FindTestDataFilePath(const std::string& file_name);

  std::unique_ptr<ScopedVAImage> Decode(
      base::span<const uint8_t> encoded_image);

  base::Lock* GetVaapiWrapperLock() const
      LOCK_RETURNED(decoder_.vaapi_wrapper_->va_lock_) {
    return decoder_.vaapi_wrapper_->va_lock_;
  }

  VADisplay GetVaapiWrapperVaDisplay() const
      EXCLUSIVE_LOCKS_REQUIRED(decoder_.vaapi_wrapper_->va_lock_) {
    return decoder_.vaapi_wrapper_->va_display_;
  }

 protected:
  std::string test_data_path_;
  VaapiJpegDecoder decoder_;
};

// Find the location of the specified test file. If a file with specified path
// is not found, treat the file as being relative to the test file directory.
// This is either a custom test data path provided by --test_data_path, or the
// default test data path (//media/test/data).
base::FilePath VaapiJpegDecoderTest::FindTestDataFilePath(
    const std::string& file_name) {
  const base::FilePath file_path = base::FilePath(file_name);
  if (base::PathExists(file_path))
    return file_path;
  if (!test_data_path_.empty())
    return base::FilePath(test_data_path_).Append(file_path);
  return GetTestDataFilePath(file_name);
}

std::unique_ptr<ScopedVAImage> VaapiJpegDecoderTest::Decode(
    base::span<const uint8_t> encoded_image) {
  VaapiJpegDecodeStatus status;
  std::unique_ptr<ScopedVAImage> scoped_image =
      decoder_.DoDecode(encoded_image, &status);
  EXPECT_EQ(!!scoped_image, status == VaapiJpegDecodeStatus::kSuccess);
  return scoped_image;
}

TEST_P(VaapiJpegDecoderTest, DecodeSuccess) {
  base::FilePath input_file = FindTestDataFilePath(GetParam().filename);
  std::string jpeg_data;
  ASSERT_TRUE(base::ReadFileToString(input_file, &jpeg_data))
      << "failed to read input data from " << input_file.value();

  const auto encoded_image = base::make_span<const uint8_t>(
      reinterpret_cast<const uint8_t*>(jpeg_data.data()), jpeg_data.size());
  std::unique_ptr<ScopedVAImage> scoped_image = Decode(encoded_image);
  ASSERT_TRUE(scoped_image);

  ASSERT_TRUE(CompareImages(encoded_image, scoped_image.get()));
}

TEST_F(VaapiJpegDecoderTest, DecodeFail) {
  // Not supported by VAAPI.
  base::FilePath input_file = FindTestDataFilePath(kUnsupportedFilename);
  std::string jpeg_data;
  ASSERT_TRUE(base::ReadFileToString(input_file, &jpeg_data))
      << "failed to read input data from " << input_file.value();
  EXPECT_FALSE(Decode(base::make_span<const uint8_t>(
      reinterpret_cast<const uint8_t*>(jpeg_data.data()), jpeg_data.size())));
}

// This test exercises the usual ScopedVAImage lifetime.
//
// TODO(andrescj): move ScopedVAImage and ScopedVABufferMapping to a separate
// file so that we don't have to use |decoder_.vaapi_wrapper_|. See
// https://crbug.com/924310.
TEST_F(VaapiJpegDecoderTest, ScopedVAImage) {
  std::vector<VASurfaceID> va_surfaces;
  const gfx::Size coded_size(64, 64);
  ASSERT_TRUE(decoder_.vaapi_wrapper_->CreateContextAndSurfaces(
      VA_RT_FORMAT_YUV420, coded_size, 1, &va_surfaces));
  ASSERT_EQ(va_surfaces.size(), 1u);

  std::unique_ptr<ScopedVAImage> scoped_image;
  {
    // On Stoney-Ridge devices the output image format is dependent on the
    // surface format. However when DoDecode() is not called the output image
    // format seems to default to I420. https://crbug.com/828119
    VAImageFormat va_image_format = kImageFormatI420;
    base::AutoLock auto_lock(*GetVaapiWrapperLock());
    scoped_image = std::make_unique<ScopedVAImage>(
        GetVaapiWrapperLock(), GetVaapiWrapperVaDisplay(), va_surfaces[0],
        &va_image_format, coded_size);

    EXPECT_TRUE(scoped_image->image());
    ASSERT_TRUE(scoped_image->IsValid());
    EXPECT_TRUE(scoped_image->va_buffer()->IsValid());
    EXPECT_TRUE(scoped_image->va_buffer()->data());
  }
}

// This test exercises creation of a ScopedVAImage with a bad VASurfaceID.
TEST_F(VaapiJpegDecoderTest, BadScopedVAImage) {
#if DCHECK_IS_ON()
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
#endif

  const std::vector<VASurfaceID> va_surfaces = {VA_INVALID_ID};
  const gfx::Size coded_size(64, 64);

  std::unique_ptr<ScopedVAImage> scoped_image;
  {
    VAImageFormat va_image_format = kImageFormatI420;
    base::AutoLock auto_lock(*GetVaapiWrapperLock());
    scoped_image = std::make_unique<ScopedVAImage>(
        GetVaapiWrapperLock(), GetVaapiWrapperVaDisplay(), va_surfaces[0],
        &va_image_format, coded_size);

    EXPECT_TRUE(scoped_image->image());
    EXPECT_FALSE(scoped_image->IsValid());
#if DCHECK_IS_ON()
    EXPECT_DCHECK_DEATH(scoped_image->va_buffer());
#else
    EXPECT_FALSE(scoped_image->va_buffer());
#endif
  }
}

// This test exercises creation of a ScopedVABufferMapping with bad VABufferIDs.
TEST_F(VaapiJpegDecoderTest, BadScopedVABufferMapping) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  base::AutoLock auto_lock(*GetVaapiWrapperLock());

  // A ScopedVABufferMapping with a VA_INVALID_ID VABufferID is DCHECK()ed.
  EXPECT_DCHECK_DEATH(std::make_unique<ScopedVABufferMapping>(
      GetVaapiWrapperLock(), GetVaapiWrapperVaDisplay(), VA_INVALID_ID));

  // This should not hit any DCHECK() but will create an invalid
  // ScopedVABufferMapping.
  auto scoped_buffer = std::make_unique<ScopedVABufferMapping>(
      GetVaapiWrapperLock(), GetVaapiWrapperVaDisplay(), VA_INVALID_ID - 1);
  EXPECT_FALSE(scoped_buffer->IsValid());
}

INSTANTIATE_TEST_SUITE_P(, VaapiJpegDecoderTest, testing::ValuesIn(kTestCases));

}  // namespace media
