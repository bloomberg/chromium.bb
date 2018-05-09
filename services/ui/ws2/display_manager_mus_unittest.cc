// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws2/display_manager_mus.h"

#include <stdint.h>

#include "base/strings/string_number_conversions.h"
#include "services/ui/common/task_runner_test_base.h"
#include "services/ui/public/interfaces/display_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/screen_base.h"

using display::Display;
using display::DisplayList;
using display::DisplayObserver;

namespace ui {
namespace ws2 {
namespace {

std::string DisplayIdsToString(
    const std::vector<mojom::WsDisplayPtr>& wm_displays) {
  std::string display_ids;
  for (const auto& wm_display : wm_displays) {
    if (!display_ids.empty())
      display_ids += " ";
    display_ids += base::NumberToString(wm_display->display.id());
  }
  return display_ids;
}

// A testing screen that generates the OnDidProcessDisplayChanges() notification
// similar to production code.
class TestScreen : public display::ScreenBase {
 public:
  TestScreen() { display::Screen::SetScreenInstance(this); }

  ~TestScreen() override { display::Screen::SetScreenInstance(nullptr); }

  void AddDisplay(const Display& display, DisplayList::Type type) {
    display_list().AddDisplay(display, type);
    for (DisplayObserver& observer : *display_list().observers())
      observer.OnDidProcessDisplayChanges();
  }

  void UpdateDisplay(const Display& display, DisplayList::Type type) {
    display_list().UpdateDisplay(display, type);
    for (DisplayObserver& observer : *display_list().observers())
      observer.OnDidProcessDisplayChanges();
  }

  void RemoveDisplay(int64_t display_id) {
    display_list().RemoveDisplay(display_id);
    for (DisplayObserver& observer : *display_list().observers())
      observer.OnDidProcessDisplayChanges();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestScreen);
};

class TestDisplayManagerObserver : public mojom::DisplayManagerObserver {
 public:
  TestDisplayManagerObserver() = default;
  ~TestDisplayManagerObserver() override = default;

  mojom::DisplayManagerObserverPtr GetPtr() {
    mojom::DisplayManagerObserverPtr ptr;
    binding_.Bind(mojo::MakeRequest(&ptr));
    return ptr;
  }

  // mojom::DisplayManagerObserver:
  void OnDisplaysChanged(std::vector<mojom::WsDisplayPtr> displays,
                         int64_t primary_display_id,
                         int64_t internal_display_id) override {
    display_ids_ = DisplayIdsToString(displays);
    primary_display_id_ = primary_display_id;
    internal_display_id_ = internal_display_id;
  }

  mojo::Binding<mojom::DisplayManagerObserver> binding_{this};
  std::string display_ids_;
  int64_t primary_display_id_ = 0;
  int64_t internal_display_id_ = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestDisplayManagerObserver);
};

// Mojo needs a task runner.
using DisplayManagerMusTest = TaskRunnerTestBase;

TEST_F(DisplayManagerMusTest, AddRemoveDisplay) {
  TestScreen screen;
  screen.AddDisplay(Display(111, gfx::Rect(0, 0, 640, 480)),
                    DisplayList::Type::PRIMARY);
  Display::SetInternalDisplayId(111);

  DisplayManagerMus manager;
  TestDisplayManagerObserver observer;

  // Adding an observer triggers an update.
  manager.AddObserver(observer.GetPtr());
  // Wait for mojo message to observer.
  RunUntilIdle();
  EXPECT_EQ("111", observer.display_ids_);
  EXPECT_EQ(111, observer.primary_display_id_);
  EXPECT_EQ(111, observer.internal_display_id_);
  observer.display_ids_.clear();

  // Adding a display triggers an update.
  screen.AddDisplay(Display(222, gfx::Rect(640, 0, 640, 480)),
                    DisplayList::Type::NOT_PRIMARY);
  // Wait for mojo message to observer.
  RunUntilIdle();
  EXPECT_EQ("111 222", observer.display_ids_);
  EXPECT_EQ(111, observer.primary_display_id_);
  observer.display_ids_.clear();

  // Updating which display is primary triggers an update.
  screen.UpdateDisplay(Display(222, gfx::Rect(640, 0, 800, 600)),
                       DisplayList::Type::PRIMARY);
  // Wait for mojo message to observer.
  RunUntilIdle();
  EXPECT_EQ("111 222", observer.display_ids_);
  EXPECT_EQ(222, observer.primary_display_id_);
  observer.display_ids_.clear();

  // Removing a display triggers an update.
  screen.RemoveDisplay(111);
  // Wait for mojo message to observer.
  RunUntilIdle();
  EXPECT_EQ("222", observer.display_ids_);
  EXPECT_EQ(222, observer.primary_display_id_);
}

}  // namespace
}  // namespace ws2
}  // namespace ui
