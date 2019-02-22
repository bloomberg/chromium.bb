// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_focus_ring_controller.h"
#include "ash/accessibility/accessibility_cursor_ring_layer.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"

namespace ash {

class TestableAccessibilityFocusRingController
    : public AccessibilityFocusRingController {
 public:
  TestableAccessibilityFocusRingController() {}
  ~TestableAccessibilityFocusRingController() override = default;

  static void GetColorAndOpacityFromColor(SkColor color,
                                          float default_opacity,
                                          SkColor* result_color,
                                          float* result_opacity) {
    AccessibilityFocusRingController::GetColorAndOpacityFromColor(
        color, default_opacity, result_color, result_opacity);
  }
};

class AccessibilityFocusRingControllerTest : public AshTestBase {
 public:
  AccessibilityFocusRingControllerTest() = default;
  ~AccessibilityFocusRingControllerTest() override = default;

 protected:
  TestableAccessibilityFocusRingController controller_;
};

TEST_F(AccessibilityFocusRingControllerTest, CallingSetOrResetWhenEmpty) {
  // Ensure that calling reset does not crash the controller if there are
  // no focus rings yet for a given ID.
  controller_.ResetFocusRingColor("catsRCute");
  controller_.HideFocusRing("catsRCute");
}

TEST_F(AccessibilityFocusRingControllerTest,
       SetFocusRingOrColorUpdatesCorrectRingGroup) {
  EXPECT_EQ(nullptr, controller_.GetFocusRingGroupForTesting("catsRCute"));
  controller_.SetFocusRingColor(SkColorSetARGB(0xFF, 0x42, 0x42, 0x42),
                                "catsRCute");
  // A focus ring group was created, but it has no layers yet.
  ASSERT_NE(nullptr, controller_.GetFocusRingGroupForTesting("catsRCute"));
  int size = controller_.GetFocusRingGroupForTesting("catsRCute")
                 ->focus_layers_for_testing()
                 .size();
  EXPECT_EQ(0, size);

  EXPECT_EQ(nullptr, controller_.GetFocusRingGroupForTesting("dogsRCool"));
  std::vector<gfx::Rect> rects;
  rects.push_back(gfx::Rect(10, 30, 70, 150));
  controller_.SetFocusRing(rects, mojom::FocusRingBehavior::FADE_OUT_FOCUS_RING,
                           "dogsRCool");
  ASSERT_NE(nullptr, controller_.GetFocusRingGroupForTesting("dogsRCool"));
  size = controller_.GetFocusRingGroupForTesting("dogsRCool")
             ->focus_layers_for_testing()
             .size();
  EXPECT_EQ(1, size);
  // The first focus ring group was not updated.
  size = controller_.GetFocusRingGroupForTesting("catsRCute")
             ->focus_layers_for_testing()
             .size();
  EXPECT_EQ(0, size);
}

TEST_F(AccessibilityFocusRingControllerTest, CursorWorksOnMultipleDisplays) {
  UpdateDisplay("400x400,500x500");
  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());

  // Simulate a mouse event on the primary display.
  AccessibilityFocusRingController* controller =
      Shell::Get()->accessibility_focus_ring_controller();
  gfx::Point location(90, 90);
  controller->SetCursorRing(location);
  AccessibilityCursorRingLayer* cursor_layer =
      controller->cursor_layer_for_testing();
  EXPECT_EQ(root_windows[0], cursor_layer->root_window());
  EXPECT_LT(abs(cursor_layer->layer()->GetTargetBounds().x() - location.x()),
            50);
  EXPECT_LT(abs(cursor_layer->layer()->GetTargetBounds().y() - location.y()),
            50);

  // Simulate a mouse event at the same local location on the secondary display.
  gfx::Point location_on_secondary = location;
  location_on_secondary.Offset(400, 0);
  controller->SetCursorRing(location_on_secondary);

  cursor_layer = controller->cursor_layer_for_testing();
  EXPECT_EQ(root_windows[1], cursor_layer->root_window());
  EXPECT_LT(abs(cursor_layer->layer()->GetTargetBounds().x() - location.x()),
            50);
  EXPECT_LT(abs(cursor_layer->layer()->GetTargetBounds().y() - location.y()),
            50);
}

TEST_F(AccessibilityFocusRingControllerTest, HighlightColorCalculation) {
  SkColor without_alpha = SkColorSetARGB(0xFF, 0x42, 0x42, 0x42);
  SkColor with_alpha = SkColorSetARGB(0x3D, 0x14, 0x15, 0x92);

  float default_opacity = 0.3f;
  SkColor result_color = SK_ColorWHITE;
  float result_opacity = 0.0f;

  TestableAccessibilityFocusRingController::GetColorAndOpacityFromColor(
      without_alpha, default_opacity, &result_color, &result_opacity);
  EXPECT_EQ(default_opacity, result_opacity);

  TestableAccessibilityFocusRingController::GetColorAndOpacityFromColor(
      with_alpha, default_opacity, &result_color, &result_opacity);
  EXPECT_NEAR(0.239f, result_opacity, .001);
}

}  // namespace ash
