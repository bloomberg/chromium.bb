// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/screen_mus.h"

#include "base/command_line.h"
#include "base/test/scoped_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display_switches.h"
#include "ui/display/screen.h"
#include "ui/views/test/scoped_views_test_helper.h"

namespace views {
namespace {

std::vector<ui::mojom::WsDisplayPtr> ConvertDisplayToWsDisplays(
    const std::vector<display::Display>& displays) {
  std::vector<ui::mojom::WsDisplayPtr> results;
  for (const auto& display : displays) {
    ui::mojom::WsDisplayPtr display_ptr = ui::mojom::WsDisplay::New();
    display_ptr->display = display;
    display_ptr->frame_decoration_values =
        ui::mojom::FrameDecorationValues::New();
    results.push_back(std::move(display_ptr));
  }
  return results;
}

TEST(ScreenMusScaleFactorTest, ConsistentDisplayInHighDPI) {
  base::test::ScopedTaskEnvironment task_environment(
      base::test::ScopedTaskEnvironment::MainThreadType::UI);
  // Must be set before |test_helper| is constructed.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kForceDeviceScaleFactor, "2");
  ScopedViewsTestHelper test_helper;
  display::Screen* screen = display::Screen::GetScreen();
  std::vector<display::Display> displays = screen->GetAllDisplays();
  ASSERT_FALSE(displays.empty());
  for (const display::Display& display : displays) {
    EXPECT_EQ(2.f, display.device_scale_factor());
    EXPECT_EQ(display.work_area(), display.bounds());
  }
}

class ScreenMusTest : public testing::Test {
 public:
  ScreenMusTest() = default;
  ~ScreenMusTest() override = default;

 private:
  base::test::ScopedTaskEnvironment task_environment_{
      base::test::ScopedTaskEnvironment::MainThreadType::UI};
  ScopedViewsTestHelper test_helper_;

  DISALLOW_COPY_AND_ASSIGN(ScreenMusTest);
};

TEST_F(ScreenMusTest, PrimaryChangedToExisting) {
  ScreenMus* screen = static_cast<ScreenMus*>(display::Screen::GetScreen());
  std::vector<display::Display> displays = screen->GetAllDisplays();
  ASSERT_FALSE(displays.empty());

  // Convert to a single display with a different primary id.
  displays.resize(1);
  displays[0].set_id(displays[0].id() + 1);
  screen->OnDisplaysChanged(ConvertDisplayToWsDisplays(displays),
                            displays[0].id(), 0, 0);
  ASSERT_EQ(1u, screen->GetAllDisplays().size());
  EXPECT_EQ(displays[0].id(), screen->GetAllDisplays()[0].id());
  EXPECT_EQ(displays[0].id(), screen->GetPrimaryDisplay().id());
}

TEST_F(ScreenMusTest, AddAndUpdate) {
  ScreenMus* screen = static_cast<ScreenMus*>(display::Screen::GetScreen());
  std::vector<display::Display> displays = screen->GetAllDisplays();
  ASSERT_FALSE(displays.empty());

  // Update the bounds of display 1, and add a new display.
  displays.resize(1);
  gfx::Rect new_bounds = displays[0].bounds();
  new_bounds.set_height(new_bounds.height() + 1);
  displays[0].set_bounds(new_bounds);
  displays.push_back(displays[0]);
  displays[1].set_id(displays[0].id() + 1);
  screen->OnDisplaysChanged(ConvertDisplayToWsDisplays(displays),
                            displays[1].id(), 0, 0);
  ASSERT_EQ(2u, screen->GetAllDisplays().size());
  ASSERT_TRUE(screen->display_list().FindDisplayById(displays[0].id()) !=
              screen->display_list().displays().end());
  EXPECT_EQ(new_bounds.height(), screen->display_list()
                                     .FindDisplayById(displays[0].id())
                                     ->bounds()
                                     .height());
  ASSERT_TRUE(screen->display_list().FindDisplayById(displays[1].id()) !=
              screen->display_list().displays().end());
  EXPECT_EQ(displays[1].id(), screen->GetPrimaryDisplay().id());
}

TEST_F(ScreenMusTest, SetDisplayForNewWindows) {
  ScreenMus* screen = static_cast<ScreenMus*>(display::Screen::GetScreen());

  // Set up 2 displays with display 1 as the display for new windows.
  constexpr int64_t kDisplayId1 = 111;
  constexpr int64_t kDisplayId2 = 222;
  std::vector<display::Display> displays = {display::Display(kDisplayId1),
                                            display::Display(kDisplayId2)};
  screen->OnDisplaysChanged(ConvertDisplayToWsDisplays(displays), kDisplayId1,
                            kDisplayId1,
                            kDisplayId1 /* display_id_for_new_windows */);
  EXPECT_EQ(kDisplayId1, screen->GetDisplayForNewWindows().id());

  // Set display 2 as the display for new windows.
  screen->OnDisplaysChanged(ConvertDisplayToWsDisplays(displays), kDisplayId1,
                            kDisplayId1,
                            kDisplayId2 /* display_id_for_new_windows */);
  EXPECT_EQ(kDisplayId2, screen->GetDisplayForNewWindows().id());

  // Set a bad display as the display for new windows. ScreenMus should fall
  // back to the primary display.
  screen->OnDisplaysChanged(ConvertDisplayToWsDisplays(displays), kDisplayId1,
                            kDisplayId1, 666 /* display_id_for_new_windows */);
  EXPECT_EQ(kDisplayId1, screen->GetDisplayForNewWindows().id());
}

}  // namespace
}  // namespace views
