// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_version.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/shape_detection/public/interfaces/textdetection.mojom.h"
#include "services/shape_detection/text_detection_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkImage.h"

namespace shape_detection {

namespace {

void DetectTextCallback(base::Closure quit_closure,
                        size_t* num_text_chunks,
                        std::vector<mojom::TextDetectionResultPtr> results) {
  *num_text_chunks = results.size();
  quit_closure.Run();
}

}  // namespace

class TextDetectionImplWinTest : public testing::Test {
 protected:
  TextDetectionImplWinTest() = default;
  ~TextDetectionImplWinTest() override = default;

  void SetUp() override {
    scoped_com_initializer_ = std::make_unique<base::win::ScopedCOMInitializer>(
        base::win::ScopedCOMInitializer::kMTA);
    ASSERT_TRUE(scoped_com_initializer_->Succeeded());
  }

 private:
  std::unique_ptr<base::win::ScopedCOMInitializer> scoped_com_initializer_;

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  DISALLOW_COPY_AND_ASSIGN(TextDetectionImplWinTest);
};

TEST_F(TextDetectionImplWinTest, ScanOnce) {
  // OCR not supported before Windows 10
  if (base::win::GetVersion() < base::win::VERSION_WIN10)
    return;

  mojom::TextDetectionPtr text_service;
  auto request = mojo::MakeRequest(&text_service);
  TextDetectionImpl::Create(std::move(request));

  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseColor(SK_ColorBLUE);

  base::RunLoop run_loop;
  size_t num_text_chunks = 1u;
  text_service->Detect(
      bitmap, base::BindOnce(&DetectTextCallback, run_loop.QuitClosure(),
                             &num_text_chunks));
  run_loop.Run();
  EXPECT_EQ(0u, num_text_chunks);
}

}  // namespace shape_detection
