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

}  // anonymous namespace

struct TestParams {
  bool fast_mode;
  int image_width;
  int image_height;
  const char* image_path;
  uint32_t num_faces;
} kTestParams[] = {
    {false, 120, 120, "services/test/data/mona_lisa.jpg", 1},
    {true, 120, 120, "services/test/data/mona_lisa.jpg", 1},
    {false, 240, 240, "services/test/data/the_beatles.jpg", 3},
    {true, 240, 240, "services/test/data/the_beatles.jpg", 3},
};

class FaceDetectionImplMacTest : public TestWithParam<struct TestParams> {
 public:
  ~FaceDetectionImplMacTest() override {}

  void DetectCallback(uint32_t num_faces,
                      std::vector<mojom::FaceDetectionResultPtr> results) {
    ASSERT_EQ(num_faces, results.size());
    for (const auto& face : results) {
      ASSERT_EQ(3u, face->landmarks.size());
      EXPECT_EQ(mojom::LandmarkType::EYE, face->landmarks[0]->type);
      EXPECT_EQ(mojom::LandmarkType::EYE, face->landmarks[1]->type);
      EXPECT_EQ(mojom::LandmarkType::MOUTH, face->landmarks[2]->type);
    }
    Detection();
  }
  MOCK_METHOD0(Detection, void(void));

  std::unique_ptr<FaceDetectionImplMac> impl_;
  const base::MessageLoop message_loop_;
};

TEST_F(FaceDetectionImplMacTest, CreateAndDestroy) {
  impl_ = std::make_unique<FaceDetectionImplMac>(
      shape_detection::mojom::FaceDetectorOptions::New());
}

TEST_P(FaceDetectionImplMacTest, ScanOneFace) {
  // Face detection test needs a GPU.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseGpuInTests)) {
    return;
  }

  auto options = shape_detection::mojom::FaceDetectorOptions::New();
  options->fast_mode = GetParam().fast_mode;
  impl_ = std::make_unique<FaceDetectionImplMac>(std::move(options));

  // Load image data from test directory.
  base::FilePath image_path;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &image_path));
  image_path = image_path.AppendASCII(GetParam().image_path);
  ASSERT_TRUE(base::PathExists(image_path));
  std::string image_data;
  ASSERT_TRUE(base::ReadFileToString(image_path, &image_data));

  std::unique_ptr<SkBitmap> image = gfx::JPEGCodec::Decode(
      reinterpret_cast<const uint8_t*>(image_data.data()), image_data.size());
  ASSERT_TRUE(image);
  ASSERT_EQ(GetParam().image_width, image->width());
  ASSERT_EQ(GetParam().image_height, image->height());

  const gfx::Size size(image->width(), image->height());
  const size_t num_bytes = size.GetArea() * 4 /* bytes per pixel */;
  // This assert assumes there is no padding in the bitmap's rowbytes
  ASSERT_EQ(num_bytes, image->computeByteSize());

  base::RunLoop run_loop;
  base::Closure quit_closure = run_loop.QuitClosure();
  // Send the image to Detect() and expect the response in callback.
  EXPECT_CALL(*this, Detection()).WillOnce(RunClosure(quit_closure));
  impl_->Detect(*image,
                base::BindOnce(&FaceDetectionImplMacTest::DetectCallback,
                               base::Unretained(this), GetParam().num_faces));

  run_loop.Run();
}

INSTANTIATE_TEST_CASE_P(, FaceDetectionImplMacTest, ValuesIn(kTestParams));

}  // shape_detection namespace
