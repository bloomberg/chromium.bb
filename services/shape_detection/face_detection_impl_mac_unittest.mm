// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/face_detection_impl_mac.h"

#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gl/gl_switches.h"

using ::testing::TestWithParam;
using ::testing::ValuesIn;

namespace shape_detection {

namespace {

ACTION_P(RunClosure, closure) {
  closure.Run();
}

const int kJpegImageWidth = 120;
const int kJpegImageHeight = 120;
const char kJpegImagePath[] = "services/test/data/mona_lisa.jpg";

}  // anonymous namespace

class FaceDetectionImplMacTest : public TestWithParam<bool> {
 public:
  ~FaceDetectionImplMacTest() override {}

  void DetectCallback(std::vector<mojom::FaceDetectionResultPtr> results) {
    ASSERT_EQ(1u, results.size());
    ASSERT_EQ(3u, results[0]->landmarks.size());
    EXPECT_EQ(mojom::LandmarkType::EYE, results[0]->landmarks[0]->type);
    EXPECT_EQ(mojom::LandmarkType::EYE, results[0]->landmarks[1]->type);
    EXPECT_EQ(mojom::LandmarkType::MOUTH, results[0]->landmarks[2]->type);
    Detection();
  }
  MOCK_METHOD0(Detection, void(void));

  std::unique_ptr<FaceDetectionImplMac> impl_;
  const base::MessageLoop message_loop_;
};

TEST_F(FaceDetectionImplMacTest, CreateAndDestroy) {
  impl_ = base::MakeUnique<FaceDetectionImplMac>(
      shape_detection::mojom::FaceDetectorOptions::New());
}

TEST_P(FaceDetectionImplMacTest, ScanOneFace) {
  // Face detection test needs a GPU.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseGpuInTests)) {
    return;
  }

  auto options = shape_detection::mojom::FaceDetectorOptions::New();
  options->fast_mode = GetParam();
  impl_ = base::MakeUnique<FaceDetectionImplMac>(std::move(options));

  // Load image data from test directory.
  base::FilePath image_path;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &image_path));
  image_path = image_path.AppendASCII(kJpegImagePath);
  ASSERT_TRUE(base::PathExists(image_path));
  std::string image_data;
  ASSERT_TRUE(base::ReadFileToString(image_path, &image_data));

  std::unique_ptr<SkBitmap> image = gfx::JPEGCodec::Decode(
      reinterpret_cast<const uint8_t*>(image_data.data()), image_data.size());
  ASSERT_TRUE(image);
  ASSERT_EQ(kJpegImageWidth, image->width());
  ASSERT_EQ(kJpegImageHeight, image->height());

  const gfx::Size size(image->width(), image->height());
  const size_t num_bytes = size.GetArea() * 4 /* bytes per pixel */;
  // This assert assumes there is no padding in the bitmap's rowbytes
  ASSERT_EQ(num_bytes, image->computeByteSize());

  base::RunLoop run_loop;
  base::Closure quit_closure = run_loop.QuitClosure();
  // Send the image to Detect() and expect the response in callback.
  EXPECT_CALL(*this, Detection()).WillOnce(RunClosure(quit_closure));
  impl_->Detect(*image, base::Bind(&FaceDetectionImplMacTest::DetectCallback,
                                   base::Unretained(this)));

  run_loop.Run();
}

INSTANTIATE_TEST_CASE_P(, FaceDetectionImplMacTest, ValuesIn({true, false}));

}  // shape_detection namespace
