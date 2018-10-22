// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_finder.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/compositor/layer_type.h"

namespace ash {
namespace wm {

using WindowFinderTest = AshTestBase;

TEST_F(WindowFinderTest, FindTopmostWindows) {
  std::unique_ptr<aura::Window> window1 =
      CreateTestWindow(gfx::Rect(0, 0, 100, 100));
  std::unique_ptr<aura::Window> window2 =
      CreateTestWindow(gfx::Rect(50, 0, 100, 100));
  std::unique_ptr<aura::Window> window3 =
      CreateTestWindow(gfx::Rect(0, 50, 100, 100));

  // Those windows shouldn't be LAYER_TEXTURED -- to check the behavior of
  // IsTopLevelWindow().
  ASSERT_NE(ui::LAYER_TEXTURED, window1->layer()->type());
  ASSERT_NE(ui::LAYER_TEXTURED, window2->layer()->type());
  ASSERT_NE(ui::LAYER_TEXTURED, window3->layer()->type());

  std::set<aura::Window*> ignore;
  ignore.insert(window3.get());

  aura::Window* real_topmost = nullptr;
  EXPECT_EQ(window1.get(),
            GetTopmostWindowAtPoint(gfx::Point(10, 10), ignore, &real_topmost));
  EXPECT_EQ(window1.get(), real_topmost);

  EXPECT_EQ(window2.get(),
            GetTopmostWindowAtPoint(gfx::Point(60, 10), ignore, &real_topmost));
  EXPECT_EQ(window2.get(), real_topmost);

  EXPECT_EQ(window2.get(),
            GetTopmostWindowAtPoint(gfx::Point(60, 60), ignore, &real_topmost));
  EXPECT_EQ(window3.get(), real_topmost);

  EXPECT_EQ(window1.get(),
            GetTopmostWindowAtPoint(gfx::Point(10, 60), ignore, &real_topmost));
  EXPECT_EQ(window3.get(), real_topmost);

  window1->parent()->StackChildAtTop(window1.get());
  EXPECT_EQ(window1.get(),
            GetTopmostWindowAtPoint(gfx::Point(60, 10), ignore, &real_topmost));
  EXPECT_EQ(window1.get(), real_topmost);

  ignore.clear();
  ignore.insert(window1.get());
  aura::Window* wallpaper_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_WallpaperContainer);
  aura::Window* topmost =
      GetTopmostWindowAtPoint(gfx::Point(10, 10), ignore, &real_topmost);
  EXPECT_TRUE(wallpaper_container->Contains(topmost));
  EXPECT_EQ(ui::LAYER_TEXTURED, topmost->layer()->type());
  EXPECT_EQ(window1.get(), real_topmost);
}

TEST_F(WindowFinderTest, RealTopmostCanBeNullptr) {
  std::unique_ptr<aura::Window> window1 =
      CreateTestWindow(gfx::Rect(0, 0, 100, 100));
  std::set<aura::Window*> ignore;

  EXPECT_EQ(window1.get(),
            GetTopmostWindowAtPoint(gfx::Point(10, 10), ignore, nullptr));
}

TEST_F(WindowFinderTest, MultipleDisplays) {
  UpdateDisplay("200x200,300x300");

  std::unique_ptr<aura::Window> window1 =
      CreateTestWindow(gfx::Rect(0, 0, 100, 100));
  std::unique_ptr<aura::Window> window2 =
      CreateTestWindow(gfx::Rect(200, 0, 100, 100));
  ASSERT_NE(window1->GetRootWindow(), window2->GetRootWindow());

  std::set<aura::Window*> ignore;
  EXPECT_EQ(window1.get(),
            GetTopmostWindowAtPoint(gfx::Point(10, 10), ignore, nullptr));
  EXPECT_EQ(window2.get(),
            GetTopmostWindowAtPoint(gfx::Point(210, 10), ignore, nullptr));
  EXPECT_EQ(nullptr,
            GetTopmostWindowAtPoint(gfx::Point(10, 210), ignore, nullptr));
}

}  // namespace wm
}  // namespace ash
