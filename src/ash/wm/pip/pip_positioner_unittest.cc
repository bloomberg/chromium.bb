// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/pip/pip_positioner.h"

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/pip/pip_test_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "ui/aura/window.h"
#include "ui/display/scoped_display_for_new_windows.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

display::Display GetDisplayForWindow(aura::Window* window) {
  return display::Screen::GetScreen()->GetDisplayNearestWindow(window);
}

gfx::Rect ConvertToScreenForWindow(aura::Window* window,
                                   const gfx::Rect& bounds) {
  gfx::Rect new_bounds = bounds;
  ::wm::ConvertRectToScreen(window->GetRootWindow(), &new_bounds);
  return new_bounds;
}

}  // namespace

class PipPositionerDisplayTest : public AshTestBase,
                                 public ::testing::WithParamInterface<
                                     std::tuple<std::string, std::size_t>> {
 public:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    AshTestBase::SetUp();

    const std::string& display_string = std::get<0>(GetParam());
    const std::size_t root_window_index = std::get<1>(GetParam());
    UpdateWorkArea(display_string);
    ASSERT_LT(root_window_index, Shell::GetAllRootWindows().size());
    root_window_ = Shell::GetAllRootWindows()[root_window_index];
    scoped_display_ =
        std::make_unique<display::ScopedDisplayForNewWindows>(root_window_);
    ForceHideShelvesForTest();
  }

  void TearDown() override {
    scoped_display_.reset();
    AshTestBase::TearDown();
  }

 protected:
  display::Display GetDisplay() { return GetDisplayForWindow(root_window_); }

  aura::Window* root_window() { return root_window_; }

  gfx::Rect ConvertToScreen(const gfx::Rect& bounds) {
    return ConvertToScreenForWindow(root_window_, bounds);
  }

  void UpdateWorkArea(const std::string& bounds) {
    UpdateDisplay(bounds);
    for (aura::Window* root : Shell::GetAllRootWindows())
      Shell::Get()->SetDisplayWorkAreaInsets(root, gfx::Insets());
  }

 private:
  std::unique_ptr<display::ScopedDisplayForNewWindows> scoped_display_;
  aura::Window* root_window_;
};

TEST_P(PipPositionerDisplayTest, PipAdjustPositionForDragClampsToMovementArea) {
  auto display = GetDisplay();

  // Adjust near top edge outside movement area.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(100, 8, 100, 100)),
            PipPositioner::GetBoundsForDrag(
                display, ConvertToScreen(gfx::Rect(100, -50, 100, 100))));

  // Adjust near bottom edge outside movement area.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(100, 292, 100, 100)),
            PipPositioner::GetBoundsForDrag(
                display, ConvertToScreen(gfx::Rect(100, 450, 100, 100))));

  // Adjust near left edge outside movement area.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(8, 100, 100, 100)),
            PipPositioner::GetBoundsForDrag(
                display, ConvertToScreen(gfx::Rect(-50, 100, 100, 100))));

  // Adjust near right edge outside movement area.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(292, 100, 100, 100)),
            PipPositioner::GetBoundsForDrag(
                display, ConvertToScreen(gfx::Rect(450, 100, 100, 100))));
}

TEST_P(PipPositionerDisplayTest,
       PipDismissedPositionDoesNotMoveAnExcessiveDistance) {
  auto display = GetDisplay();

  EXPECT_EQ(ConvertToScreen(gfx::Rect(100, 100, 100, 100)),
            PipPositioner::GetDismissedPosition(
                display, ConvertToScreen(gfx::Rect(100, 100, 100, 100))));
}

TEST_P(PipPositionerDisplayTest, PipDismissedPositionChosesClosestEdge) {
  auto display = GetDisplay();

  // Dismiss near top edge outside movement area towards top.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(100, -100, 100, 100)),
            PipPositioner::GetDismissedPosition(
                display, ConvertToScreen(gfx::Rect(100, 50, 100, 100))));

  // Dismiss near bottom edge outside movement area towards bottom.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(100, 400, 100, 100)),
            PipPositioner::GetDismissedPosition(
                display, ConvertToScreen(gfx::Rect(100, 250, 100, 100))));

  // Dismiss near left edge outside movement area towards left.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(-100, 100, 100, 100)),
            PipPositioner::GetDismissedPosition(
                display, ConvertToScreen(gfx::Rect(50, 100, 100, 100))));

  // Dismiss near right edge outside movement area towards right.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(400, 100, 100, 100)),
            PipPositioner::GetDismissedPosition(
                display, ConvertToScreen(gfx::Rect(250, 100, 100, 100))));
}

// Verify that if two edges are equally close, the PIP window prefers dismissing
// out horizontally.
TEST_P(PipPositionerDisplayTest, PipDismissedPositionPrefersHorizontal) {
  auto display = GetDisplay();

  // Top left corner.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(-150, 0, 100, 100)),
            PipPositioner::GetDismissedPosition(
                display, ConvertToScreen(gfx::Rect(0, 0, 100, 100))));

  // Top right corner.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(450, 0, 100, 100)),
            PipPositioner::GetDismissedPosition(
                display, ConvertToScreen(gfx::Rect(300, 0, 100, 100))));

  // Bottom left corner.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(-150, 300, 100, 100)),
            PipPositioner::GetDismissedPosition(
                display, ConvertToScreen(gfx::Rect(0, 300, 100, 100))));

  // Bottom right corner.
  EXPECT_EQ(ConvertToScreen(gfx::Rect(450, 300, 100, 100)),
            PipPositioner::GetDismissedPosition(
                display, ConvertToScreen(gfx::Rect(300, 300, 100, 100))));
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    PipPositionerDisplayTest,
    testing::Values(std::make_tuple("400x400", 0u),
                    std::make_tuple("400x400/r", 0u),
                    std::make_tuple("400x400/u", 0u),
                    std::make_tuple("400x400/l", 0u),
                    std::make_tuple("800x800*2", 0u),
                    std::make_tuple("400x400,400x400", 0u),
                    std::make_tuple("400x400,400x400", 1u)));

}  // namespace ash
