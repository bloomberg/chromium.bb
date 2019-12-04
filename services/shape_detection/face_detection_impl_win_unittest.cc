// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_version.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/shape_detection/face_detection_provider_win.h"
#include "services/shape_detection/public/mojom/facedetection_provider.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace shape_detection {

namespace {
void DetectCallback(base::OnceClosure quit_closure,
                    uint32_t* num_faces,
                    std::vector<mojom::FaceDetectionResultPtr> results) {
  *num_faces = results.size();
  std::move(quit_closure).Run();
}
}  // namespace

class FaceDetectionImplWinTest : public testing::Test {
 protected:
  FaceDetectionImplWinTest() = default;
  ~FaceDetectionImplWinTest() override = default;

  void SetUp() override {
    scoped_com_initializer_ = std::make_unique<base::win::ScopedCOMInitializer>(
        base::win::ScopedCOMInitializer::kMTA);
    ASSERT_TRUE(scoped_com_initializer_->Succeeded());
  }

  mojo::Remote<mojom::FaceDetection> ConnectToFaceDetector() {
    mojo::Remote<mojom::FaceDetectionProvider> provider;
    mojo::Remote<mojom::FaceDetection> face_service;

    FaceDetectionProviderWin::Create(provider.BindNewPipeAndPassReceiver());

    auto options = shape_detection::mojom::FaceDetectorOptions::New();
    provider->CreateFaceDetection(face_service.BindNewPipeAndPassReceiver(),
                                  std::move(options));

    return face_service;
  }

  std::unique_ptr<SkBitmap> LoadTestImage() {
    // Load image data from test directory.
    base::FilePath image_path;
    EXPECT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &image_path));
    image_path = image_path.Append(FILE_PATH_LITERAL("services"))
                     .Append(FILE_PATH_LITERAL("test"))
                     .Append(FILE_PATH_LITERAL("data"))
                     .Append(FILE_PATH_LITERAL("mona_lisa.jpg"));
    EXPECT_TRUE(base::PathExists(image_path));
    std::string image_data;
    EXPECT_TRUE(base::ReadFileToString(image_path, &image_data));

    std::unique_ptr<SkBitmap> image = gfx::JPEGCodec::Decode(
        reinterpret_cast<const uint8_t*>(image_data.data()), image_data.size());
    if (image) {
      const gfx::Size size(image->width(), image->height());
      const uint32_t num_bytes = size.GetArea() * 4 /* bytes per pixel */;
      EXPECT_EQ(num_bytes, image->computeByteSize());
    }

    return image;
  }

 private:
  std::unique_ptr<base::win::ScopedCOMInitializer> scoped_com_initializer_;

  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(FaceDetectionImplWinTest);
};

TEST_F(FaceDetectionImplWinTest, ScanOneFace) {
  // FaceDetector not supported before Windows 10
  if (base::win::GetVersion() < base::win::Version::WIN10)
    return;

  mojo::Remote<mojom::FaceDetection> face_detector = ConnectToFaceDetector();
  std::unique_ptr<SkBitmap> image = LoadTestImage();
  ASSERT_TRUE(image);

  base::RunLoop run_loop;
  uint32_t num_faces = 0;
  face_detector->Detect(
      *image,
      base::BindOnce(&DetectCallback, run_loop.QuitClosure(), &num_faces));
  run_loop.Run();
  EXPECT_EQ(1u, num_faces);
}

}  // namespace shape_detection
