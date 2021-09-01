// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/full_restore/arc_window_utils.h"

#include "ash/public/cpp/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "components/exo/wm_helper.h"
#include "components/exo/wm_helper_chromeos.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/test/test_screen.h"

namespace {
const int TEST_DISPLAY_ID = 0x1387;
const int TEST_DISPLAY_WIDTH = 2560;
const int TEST_DISPLAY_HEIGHT = 1440;
const double TEST_SCALE_FACTOR = 2.0;
}  // namespace

namespace chromeos {
namespace full_restore {

class ArcWindowUtilsTest : public testing::Test {
 protected:
  ArcWindowUtilsTest() {
    const display::Display test_display = test_screen_.GetPrimaryDisplay();
    display::Display display(test_display);
    display.set_id(TEST_DISPLAY_ID);
    display.set_bounds(
        gfx::Rect(0, 0, TEST_DISPLAY_WIDTH, TEST_DISPLAY_HEIGHT));
    display.set_device_scale_factor(TEST_SCALE_FACTOR);
    test_screen_.display_list().RemoveDisplay(test_display.id());
    test_screen_.display_list().AddDisplay(display,
                                           display::DisplayList::Type::PRIMARY);
    display::Screen::SetScreenInstance(&test_screen_);
    base::CommandLine::ForCurrentProcess()->InitFromArgv(
        {"", "--enable-arcvm"});
  }
  ArcWindowUtilsTest(const ArcWindowUtilsTest&) = delete;
  ArcWindowUtilsTest& operator=(const ArcWindowUtilsTest&) = delete;
  ~ArcWindowUtilsTest() override {
    display::Screen::SetScreenInstance(nullptr);
  }

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {ash::features::kFullRestore, ash::features::kArcGhostWindow}, {});
    wm_helper_ = std::make_unique<exo::WMHelperChromeOS>();
  }

  void TearDown() override { wm_helper_.reset(); }

 private:
  display::test::TestScreen test_screen_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<exo::WMHelper> wm_helper_;
};

TEST_F(ArcWindowUtilsTest, ArcWindowInfoInvalidDisplayValidBoundsTest) {
  apps::mojom::WindowInfoPtr window_info = apps::mojom::WindowInfo::New();

  window_info->display_id = display::kInvalidDisplayId;
  window_info->bounds = apps::mojom::Rect::New();

  auto arc_window_info = HandleArcWindowInfo(std::move(window_info));
  EXPECT_TRUE(arc_window_info->bounds.is_null());
}

TEST_F(ArcWindowUtilsTest, ArcWindowInfoValidDisplayInvalidBoundsTest) {
  apps::mojom::WindowInfoPtr window_info = apps::mojom::WindowInfo::New();

  window_info->display_id = TEST_DISPLAY_ID;

  auto arc_window_info = HandleArcWindowInfo(std::move(window_info));
  EXPECT_TRUE(arc_window_info->bounds.is_null());
}

TEST_F(ArcWindowUtilsTest, ArcWindowInfoValidDisplayAndBoundsTest) {
  apps::mojom::WindowInfoPtr window_info = apps::mojom::WindowInfo::New();
  window_info->display_id = TEST_DISPLAY_ID;
  window_info->bounds = apps::mojom::Rect::New();
  window_info->bounds->x = 100;
  window_info->bounds->y = 200;
  window_info->bounds->width = 300;
  window_info->bounds->height = 400;

  auto arc_window_info = HandleArcWindowInfo(std::move(window_info));
  EXPECT_FALSE(arc_window_info->bounds.is_null());
  EXPECT_EQ(arc_window_info->bounds->x, 200);
  EXPECT_EQ(arc_window_info->bounds->y, 400);
  EXPECT_EQ(arc_window_info->bounds->width, 600);
  EXPECT_EQ(arc_window_info->bounds->height, 800);
}

}  // namespace full_restore
}  // namespace chromeos
