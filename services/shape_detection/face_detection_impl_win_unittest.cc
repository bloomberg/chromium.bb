// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/face_detection_impl_win.h"

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/win/scoped_com_initializer.h"
#include "face_detection_impl_win.h"
#include "face_detection_provider_win.h"
#include "services/shape_detection/public/interfaces/facedetection_provider.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace shape_detection {

class FaceDetectionImplWinTest : public testing::Test {
 protected:
  void SetUp() override {
    scoped_com_initializer_ = std::make_unique<base::win::ScopedCOMInitializer>(
        base::win::ScopedCOMInitializer::kMTA);
    ASSERT_TRUE(scoped_com_initializer_->Succeeded());
  }

 public:
  void CreateFaceDetectorCallback(bool succeeded) { EXPECT_TRUE(succeeded); }

  void DetectCallback(base::Closure quit_closure,
                      std::vector<mojom::FaceDetectionResultPtr> results) {
    CloseConnection(quit_closure);
  }

  void CloseConnection(base::Closure quit_closure) { quit_closure.Run(); }

 private:
  std::unique_ptr<base::win::ScopedCOMInitializer> scoped_com_initializer_;

  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

TEST_F(FaceDetectionImplWinTest, CreateFaceDetector) {
  mojom::FaceDetectionProviderPtr provider;
  mojom::FaceDetectionPtr face_service;

  auto request = mojo::MakeRequest(&provider);
  auto providerWin = base::MakeUnique<FaceDetectionProviderWin>();
  auto* provider_ptr = providerWin.get();
  provider_ptr->binding_ =
      mojo::MakeStrongBinding(std::move(providerWin), std::move(request));

  auto options = shape_detection::mojom::FaceDetectorOptions::New();
  provider->CreateFaceDetection(mojo::MakeRequest(&face_service),
                                std::move(options));

  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseColor(SK_ColorBLUE);

  base::RunLoop run_loop;
  g_callback_for_testing =
      base::BindOnce(&FaceDetectionImplWinTest::CreateFaceDetectorCallback,
                     base::Unretained(this));

  face_service.set_connection_error_handler(
      base::Bind(&FaceDetectionImplWinTest::CloseConnection,
                 base::Unretained(this), run_loop.QuitClosure()));
  face_service->Detect(
      bitmap, base::BindOnce(&FaceDetectionImplWinTest::DetectCallback,
                             base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace shape_detection
