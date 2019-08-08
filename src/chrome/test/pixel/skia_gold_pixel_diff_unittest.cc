// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/pixel/skia_gold_pixel_diff.h"

#include "third_party/skia/include/core/SkImageInfo.h"

#include "base/command_line.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/test_browser_window_aura.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/view.h"

using ::testing::Return;
using ::testing::_;

class MockView : public views::View {
 public:
  MockView() {}
  MOCK_CONST_METHOD0(GetBoundsInScreen, gfx::Rect());
};

class MockSkiaGoldPixelDiff : public SkiaGoldPixelDiff {
 public:
  MockSkiaGoldPixelDiff() {}
  MOCK_METHOD2(UploadToSkiaGoldServer,
      bool(const base::FilePath&, const std::string&));
  bool GrabWindowSnapshotInternal(gfx::NativeWindow window,
                                  const gfx::Rect& snapshot_bounds,
                                  gfx::Image* image) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(10, 10);
    *image = gfx::Image::CreateFrom1xBitmap(bitmap);
    return true;
  }
};

class SkiaGoldPixelDiffTest : public ::testing::Test {
 public:
  SkiaGoldPixelDiffTest() {
    std::unique_ptr<aura::Window> aura_win = std::make_unique<aura::Window>(
        nullptr, aura::client::WINDOW_TYPE_NORMAL);
    aura_win->Init(ui::LAYER_NOT_DRAWN);
    aura_win->SetBounds(gfx::Rect(0, 0, 10, 10));
    test_browser = new TestBrowserWindowAura(std::move(aura_win));

    auto* cmd_line = base::CommandLine::ForCurrentProcess();
    cmd_line->AppendSwitchASCII("build-revision", "test");
  }

  ~SkiaGoldPixelDiffTest() override {
   delete test_browser;
  }
 protected:
  TestBrowserWindowAura* test_browser;
  DISALLOW_COPY_AND_ASSIGN(SkiaGoldPixelDiffTest);
};

TEST_F(SkiaGoldPixelDiffTest, CompareScreenshotByView) {
  gfx::Rect rect;
  MockView mock_view;
  EXPECT_CALL(mock_view, GetBoundsInScreen()).WillRepeatedly(Return(rect));
  MockSkiaGoldPixelDiff mock_pixel;
  EXPECT_CALL(mock_pixel, UploadToSkiaGoldServer(_, "Prefix_Demo"))
      .Times(1).WillOnce(Return(true));
  mock_pixel.Init(this->test_browser, "Prefix");
  bool ret = mock_pixel.CompareScreenshot("Demo", &mock_view);
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, CompareScreenshotBySkBitmap) {
  SkBitmap bitmap;
  SkImageInfo info = SkImageInfo::Make(10, 10,
      SkColorType::kBGRA_8888_SkColorType, SkAlphaType::kPremul_SkAlphaType);
  bitmap.allocPixels(info, 10 * 4);
  MockSkiaGoldPixelDiff mock_pixel;
  EXPECT_CALL(mock_pixel, UploadToSkiaGoldServer(_, "Prefix_test"))
      .Times(1).WillOnce(Return(true));
  mock_pixel.Init(this->test_browser, "Prefix");
  bool ret = mock_pixel.CompareScreenshot("test", bitmap);
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, CompareScreenshotByViewNotInitialized) {
  SkiaGoldPixelDiff pixel_diff;
  MockView mock_view;
  bool ret = pixel_diff.CompareScreenshot("Fail", &mock_view);
  EXPECT_FALSE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, CompareScreenshotBySkBitmapNotInitialized) {
  SkBitmap bitmap;
  SkiaGoldPixelDiff pixel_diff;
  bool ret = pixel_diff.CompareScreenshot("Fail", bitmap);
  EXPECT_FALSE(ret);
}
