// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/snapshot/snapshot.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/gfx_paths.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/transform.h"
#include "ui/gl/gl_implementation.h"

namespace ui {
namespace {
const SkColor kPaintColor = SK_ColorRED;

// Paint simple rectangle on the specified aura window.
class TestPaintingWindowDelegate : public aura::test::TestWindowDelegate {
 public:
  explicit TestPaintingWindowDelegate(const gfx::Size& window_size)
      : window_size_(window_size) {
  }

  virtual ~TestPaintingWindowDelegate() {
  }

  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->FillRect(gfx::Rect(window_size_), kPaintColor);
  }

 private:
  gfx::Size window_size_;

  DISALLOW_COPY_AND_ASSIGN(TestPaintingWindowDelegate);
};

size_t GetFailedPixelsCount(const gfx::Image& image) {
  const SkBitmap* bitmap = image.ToSkBitmap();
  uint32* bitmap_data = reinterpret_cast<uint32*>(
      bitmap->pixelRef()->pixels());
  size_t result = 0;
  for (int i = 0; i < bitmap->width() * bitmap->height(); ++i) {
    if (static_cast<SkColor>(bitmap_data[i]) != kPaintColor)
      ++result;
  }
  return result;
}

}  // namespace

class SnapshotAuraTest : public testing::Test {
 public:
  SnapshotAuraTest() {}
  virtual ~SnapshotAuraTest() {}

  virtual void SetUp() OVERRIDE {
    testing::Test::SetUp();
    helper_.reset(
        new aura::test::AuraTestHelper(base::MessageLoopForUI::current()));
    helper_->SetUp();
  }

  virtual void TearDown() OVERRIDE {
    test_window_.reset();
    delegate_.reset();
    helper_->RunAllPendingInMessageLoop();
    helper_->TearDown();
    testing::Test::TearDown();
  }

 protected:
  aura::Window* test_window() { return test_window_.get(); }
  aura::RootWindow* root_window() { return helper_->root_window(); }
  aura::TestScreen* test_screen() { return helper_->test_screen(); }

  void WaitForDraw() {
    root_window()->compositor()->ScheduleDraw();
    ui::DrawWaiterForTest::Wait(root_window()->compositor());
  }

  void SetupTestWindow(const gfx::Rect& window_bounds) {
    delegate_.reset(new TestPaintingWindowDelegate(window_bounds.size()));
    test_window_.reset(aura::test::CreateTestWindowWithDelegate(
        delegate_.get(), 0, window_bounds, root_window()));
  }

  gfx::Image GrabSnapshotForTestWindow() {
    std::vector<unsigned char> png_representation;
    gfx::Rect local_bounds(test_window_->bounds().size());
    ui::GrabWindowSnapshot(test_window(), &png_representation, local_bounds);
    return gfx::Image::CreateFrom1xPNGBytes(
      &(png_representation[0]), png_representation.size());
  }

 private:
  scoped_ptr<aura::test::AuraTestHelper> helper_;
  scoped_ptr<aura::Window> test_window_;
  scoped_ptr<TestPaintingWindowDelegate> delegate_;
  std::vector<unsigned char> png_representation_;

  DISALLOW_COPY_AND_ASSIGN(SnapshotAuraTest);
};

TEST_F(SnapshotAuraTest, FullScreenWindow) {
  SetupTestWindow(root_window()->bounds());
  WaitForDraw();

  gfx::Image snapshot = GrabSnapshotForTestWindow();
  EXPECT_EQ(test_window()->bounds().size().ToString(),
            snapshot.Size().ToString());
  EXPECT_EQ(0u, GetFailedPixelsCount(snapshot));
}

TEST_F(SnapshotAuraTest, PartialBounds) {
  gfx::Rect test_bounds(100, 100, 300, 200);
  SetupTestWindow(test_bounds);
  WaitForDraw();

  gfx::Image snapshot = GrabSnapshotForTestWindow();
  EXPECT_EQ(test_bounds.size().ToString(),
            snapshot.Size().ToString());
  EXPECT_EQ(0u, GetFailedPixelsCount(snapshot));
}

TEST_F(SnapshotAuraTest, Rotated) {
  test_screen()->SetDisplayRotation(gfx::Display::ROTATE_90);

  gfx::Rect test_bounds(100, 100, 300, 200);
  SetupTestWindow(test_bounds);
  WaitForDraw();

  gfx::Image snapshot = GrabSnapshotForTestWindow();
  EXPECT_EQ(test_bounds.size().ToString(),
            snapshot.Size().ToString());
  EXPECT_EQ(0u, GetFailedPixelsCount(snapshot));
}

TEST_F(SnapshotAuraTest, UIScale) {
  const float kUIScale = 1.25f;
  test_screen()->SetUIScale(kUIScale);

  gfx::Rect test_bounds(100, 100, 300, 200);
  SetupTestWindow(test_bounds);
  WaitForDraw();

  // Snapshot always captures the physical pixels.
  gfx::SizeF snapshot_size(test_bounds.size());
  snapshot_size.Scale(1.0f / kUIScale);

  gfx::Image snapshot = GrabSnapshotForTestWindow();
  EXPECT_EQ(gfx::ToRoundedSize(snapshot_size).ToString(),
            snapshot.Size().ToString());
  EXPECT_EQ(0u, GetFailedPixelsCount(snapshot));
}

TEST_F(SnapshotAuraTest, DeviceScaleFactor) {
  test_screen()->SetDeviceScaleFactor(2.0f);

  gfx::Rect test_bounds(100, 100, 150, 100);
  SetupTestWindow(test_bounds);
  WaitForDraw();

  // Snapshot always captures the physical pixels.
  gfx::SizeF snapshot_size(test_bounds.size());
  snapshot_size.Scale(2.0f);

  gfx::Image snapshot = GrabSnapshotForTestWindow();
  EXPECT_EQ(gfx::ToRoundedSize(snapshot_size).ToString(),
            snapshot.Size().ToString());
  EXPECT_EQ(0u, GetFailedPixelsCount(snapshot));
}

TEST_F(SnapshotAuraTest, RotateAndUIScale) {
  const float kUIScale = 1.25f;
  test_screen()->SetUIScale(kUIScale);
  test_screen()->SetDisplayRotation(gfx::Display::ROTATE_90);

  gfx::Rect test_bounds(100, 100, 300, 200);
  SetupTestWindow(test_bounds);
  WaitForDraw();

  // Snapshot always captures the physical pixels.
  gfx::SizeF snapshot_size(test_bounds.size());
  snapshot_size.Scale(1.0f / kUIScale);

  gfx::Image snapshot = GrabSnapshotForTestWindow();
  EXPECT_EQ(gfx::ToRoundedSize(snapshot_size).ToString(),
            snapshot.Size().ToString());
  EXPECT_EQ(0u, GetFailedPixelsCount(snapshot));
}

TEST_F(SnapshotAuraTest, RotateAndUIScaleAndScaleFactor) {
  test_screen()->SetDeviceScaleFactor(2.0f);
  const float kUIScale = 1.25f;
  test_screen()->SetUIScale(kUIScale);
  test_screen()->SetDisplayRotation(gfx::Display::ROTATE_90);

  gfx::Rect test_bounds(20, 30, 150, 100);
  SetupTestWindow(test_bounds);
  WaitForDraw();

  // Snapshot always captures the physical pixels.
  gfx::SizeF snapshot_size(test_bounds.size());
  snapshot_size.Scale(2.0f / kUIScale);

  gfx::Image snapshot = GrabSnapshotForTestWindow();
  EXPECT_EQ(gfx::ToRoundedSize(snapshot_size).ToString(),
            snapshot.Size().ToString());
  EXPECT_EQ(0u, GetFailedPixelsCount(snapshot));
}

}  // namespace ui
