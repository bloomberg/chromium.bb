// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_drag_icon_proxy.h"

#include <memory>

#include "ash/public/cpp/image_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/bind.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/widget/widget.h"

namespace ash {

class AppDragIconProxyTest : public AshTestBase {
 public:
  AppDragIconProxyTest() = default;
  AppDragIconProxyTest(const AppDragIconProxyTest&) = delete;
  AppDragIconProxyTest& operator=(const AppDragIconProxyTest&) = delete;
  ~AppDragIconProxyTest() override = default;

  gfx::Rect GetTargetAppDragIconImageBounds(AppDragIconProxy* drag_icon_proxy) {
    gfx::RectF layer_target_position(
        drag_icon_proxy->GetImageLayerForTesting()->GetTargetBounds());
    drag_icon_proxy->GetImageLayerForTesting()
        ->GetTargetTransform()
        .TransformRect(&layer_target_position);
    gfx::Rect widget_target_bounds =
        drag_icon_proxy->GetWidgetForTesting()->GetLayer()->GetTargetBounds();
    layer_target_position.set_origin(
        gfx::PointF(layer_target_position.x() + widget_target_bounds.x(),
                    layer_target_position.y() + widget_target_bounds.y()));
    return gfx::ToNearestRect(layer_target_position);
  }
};

TEST_F(AppDragIconProxyTest, UpdatingLocationRespectsIconOffset) {
  auto drag_icon_proxy = std::make_unique<AppDragIconProxy>(
      Shell::GetPrimaryRootWindow(),
      ash::image_util::CreateEmptyImage(gfx::Size(50, 50)),
      /*pointer_location_in_screen=*/gfx::Point(100, 200),
      /*pointer_offset_from_center=*/gfx::Vector2d(10, 20),
      /*scale_factor=*/1.0f,
      /*use_blurred_background=*/false);

  EXPECT_EQ(gfx::Rect(gfx::Point(65, 155), gfx::Size(50, 50)),
            drag_icon_proxy->GetBoundsInScreen());

  drag_icon_proxy->UpdatePosition(gfx::Point(150, 200));
  EXPECT_EQ(gfx::Rect(gfx::Point(115, 155), gfx::Size(50, 50)),
            drag_icon_proxy->GetBoundsInScreen());

  drag_icon_proxy->UpdatePosition(gfx::Point(150, 250));
  EXPECT_EQ(gfx::Rect(gfx::Point(115, 205), gfx::Size(50, 50)),
            drag_icon_proxy->GetBoundsInScreen());

  drag_icon_proxy->UpdatePosition(gfx::Point(120, 270));
  EXPECT_EQ(gfx::Rect(gfx::Point(85, 225), gfx::Size(50, 50)),
            drag_icon_proxy->GetBoundsInScreen());
}

TEST_F(AppDragIconProxyTest, SecondaryDisplay) {
  UpdateDisplay("1000x800,1000x800");

  auto drag_icon_proxy = std::make_unique<AppDragIconProxy>(
      Shell::GetRootWindowForDisplayId(GetSecondaryDisplay().id()),
      ash::image_util::CreateEmptyImage(gfx::Size(50, 50)),
      /*pointer_location_in_screen=*/gfx::Point(1100, 200),
      /*pointer_offset_from_center=*/gfx::Vector2d(10, 20),
      /*scale_factor=*/1.0f,
      /*use_blurred_background=*/false);

  EXPECT_EQ(gfx::Rect(gfx::Point(1065, 155), gfx::Size(50, 50)),
            drag_icon_proxy->GetBoundsInScreen());

  drag_icon_proxy->UpdatePosition(gfx::Point(1150, 200));
  EXPECT_EQ(gfx::Rect(gfx::Point(1115, 155), gfx::Size(50, 50)),
            drag_icon_proxy->GetBoundsInScreen());

  drag_icon_proxy->UpdatePosition(gfx::Point(1150, 250));
  EXPECT_EQ(gfx::Rect(gfx::Point(1115, 205), gfx::Size(50, 50)),
            drag_icon_proxy->GetBoundsInScreen());

  drag_icon_proxy->UpdatePosition(gfx::Point(1120, 270));
  EXPECT_EQ(gfx::Rect(gfx::Point(1085, 225), gfx::Size(50, 50)),
            drag_icon_proxy->GetBoundsInScreen());
}

TEST_F(AppDragIconProxyTest, ScaledBounds) {
  auto drag_icon_proxy = std::make_unique<AppDragIconProxy>(
      Shell::GetPrimaryRootWindow(),
      ash::image_util::CreateEmptyImage(gfx::Size(50, 50)),
      /*pointer_location_in_screen=*/gfx::Point(200, 400),
      /*pointer_offset_from_center=*/gfx::Vector2d(10, 20),
      /*scale_factor=*/2.0f,
      /*use_blurred_background=*/false);

  EXPECT_EQ(gfx::Rect(gfx::Point(140, 330), gfx::Size(100, 100)),
            drag_icon_proxy->GetBoundsInScreen());

  drag_icon_proxy->UpdatePosition(gfx::Point(150, 300));
  EXPECT_EQ(gfx::Rect(gfx::Point(90, 230), gfx::Size(100, 100)),
            drag_icon_proxy->GetBoundsInScreen());
}

TEST_F(AppDragIconProxyTest, BlurSetsRoundedCorners) {
  auto drag_icon_proxy = std::make_unique<AppDragIconProxy>(
      Shell::GetPrimaryRootWindow(),
      ash::image_util::CreateEmptyImage(gfx::Size(50, 50)),
      /*pointer_location_in_screen=*/gfx::Point(100, 200),
      /*pointer_offset_from_center=*/gfx::Vector2d(10, 20),
      /*scale_factor=*/1.0f,
      /*use_blurred_background=*/true);

  // The background should be circular.
  EXPECT_EQ(gfx::RoundedCornersF(25.0f).ToString(),
            drag_icon_proxy->GetImageLayerForTesting()
                ->rounded_corner_radii()
                .ToString());
  EXPECT_EQ(30.0f,
            drag_icon_proxy->GetImageLayerForTesting()->background_blur());

  // Test that background corner radii are scaled with the image.
  drag_icon_proxy = std::make_unique<AppDragIconProxy>(
      Shell::GetPrimaryRootWindow(),
      ash::image_util::CreateEmptyImage(gfx::Size(50, 50)),
      /*pointer_location_in_screen=*/gfx::Point(100, 200),
      /*pointer_offset_from_center=*/gfx::Vector2d(10, 20),
      /*scale_factor=*/2.0f,
      /*use_blurred_background=*/true);

  // The background should be circular.
  EXPECT_EQ(gfx::RoundedCornersF(50.0f).ToString(),
            drag_icon_proxy->GetImageLayerForTesting()
                ->rounded_corner_radii()
                .ToString());
  EXPECT_EQ(30.0f,
            drag_icon_proxy->GetImageLayerForTesting()->background_blur());
}

TEST_F(AppDragIconProxyTest, AnimateBoundsForClosure) {
  auto drag_icon_proxy = std::make_unique<AppDragIconProxy>(
      Shell::GetPrimaryRootWindow(),
      ash::image_util::CreateEmptyImage(gfx::Size(50, 50)),
      /*pointer_location_in_screen=*/gfx::Point(100, 200),
      /*pointer_offset_from_center=*/gfx::Vector2d(10, 20),
      /*scale_factor=*/1.0f,
      /*use_blurred_background=*/false);
  EXPECT_EQ(gfx::Rect(gfx::Point(65, 155), gfx::Size(50, 50)),
            drag_icon_proxy->GetBoundsInScreen());

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  base::RunLoop run_loop;
  gfx::Rect target_bounds = gfx::Rect(gfx::Point(50, 50), gfx::Size(10, 10));
  drag_icon_proxy->AnimateToBoundsAndCloseWidget(target_bounds,
                                                 run_loop.QuitClosure());

  EXPECT_EQ(gfx::Rect(gfx::Point(65, 155), gfx::Size(50, 50)),
            drag_icon_proxy->GetBoundsInScreen());
  EXPECT_EQ(target_bounds,
            GetTargetAppDragIconImageBounds(drag_icon_proxy.get()));

  // Verify the animation callback gets called.
  run_loop.Run();

  // The widget should be closed at this point - verify public methods do no
  // crash if used.
  EXPECT_EQ(gfx::Rect(), drag_icon_proxy->GetBoundsInScreen());
  drag_icon_proxy->UpdatePosition(gfx::Point(20, 20));
  EXPECT_EQ(gfx::Rect(), drag_icon_proxy->GetBoundsInScreen());
  drag_icon_proxy->SetOpacity(1.0f);
}

TEST_F(AppDragIconProxyTest, CloseAnimationCallbackCalledWithZeroAnimation) {
  auto drag_icon_proxy = std::make_unique<AppDragIconProxy>(
      Shell::GetPrimaryRootWindow(),
      ash::image_util::CreateEmptyImage(gfx::Size(50, 50)),
      /*pointer_location_in_screen=*/gfx::Point(100, 200),
      /*pointer_offset_from_center=*/gfx::Vector2d(10, 20),
      /*scale_factor=*/1.0f,
      /*use_blurred_background=*/false);
  EXPECT_EQ(gfx::Rect(gfx::Point(65, 155), gfx::Size(50, 50)),
            drag_icon_proxy->GetBoundsInScreen());

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  bool animation_callback_called = false;
  gfx::Rect target_bounds = gfx::Rect(gfx::Point(50, 50), gfx::Size(10, 10));
  drag_icon_proxy->AnimateToBoundsAndCloseWidget(
      target_bounds, base::BindLambdaForTesting([&animation_callback_called]() {
        animation_callback_called = true;
      }));
  EXPECT_TRUE(animation_callback_called);
  // The widget should be closed at this point - verify public methods do no
  // crash if used.
  EXPECT_EQ(gfx::Rect(), drag_icon_proxy->GetBoundsInScreen());
  drag_icon_proxy->UpdatePosition(gfx::Point(20, 20));
  EXPECT_EQ(gfx::Rect(), drag_icon_proxy->GetBoundsInScreen());
  drag_icon_proxy->SetOpacity(1.0f);
}

// Tests the timing of animation callback if app drag icon is deleted while
// another layer animation is in progress.
TEST_F(AppDragIconProxyTest,
       CloseAnimationInterruptsAnotherLayerTransformAnimation) {
  auto drag_icon_proxy = std::make_unique<AppDragIconProxy>(
      Shell::GetPrimaryRootWindow(),
      ash::image_util::CreateEmptyImage(gfx::Size(50, 50)),
      /*pointer_location_in_screen=*/gfx::Point(100, 200),
      /*pointer_offset_from_center=*/gfx::Vector2d(10, 20),
      /*scale_factor=*/1.0f,
      /*use_blurred_background=*/false);
  EXPECT_EQ(gfx::Rect(gfx::Point(65, 155), gfx::Size(50, 50)),
            drag_icon_proxy->GetBoundsInScreen());

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Animate the drag image layer transform, so
  // `AnimateToBoundsAndCloseWidget()` interrupts an animation.
  {
    ui::ScopedLayerAnimationSettings animation_settings(
        drag_icon_proxy->GetImageLayerForTesting()->GetAnimator());
    animation_settings.SetTransitionDuration(base::Milliseconds(300));
    gfx::Transform transform;
    transform.Translate(100, 100);
    drag_icon_proxy->GetImageLayerForTesting()->SetTransform(transform);
  }

  base::RunLoop run_loop;
  bool animation_callback_called = false;
  gfx::Rect target_bounds = gfx::Rect(gfx::Point(50, 50), gfx::Size(10, 10));
  drag_icon_proxy->AnimateToBoundsAndCloseWidget(
      target_bounds,
      base::BindLambdaForTesting([&animation_callback_called, &run_loop]() {
        animation_callback_called = true;
        run_loop.Quit();
      }));

  // The animation callback should not yet been called, as the close animation
  // has not finished.
  ASSERT_FALSE(animation_callback_called);

  EXPECT_EQ(gfx::Rect(gfx::Point(65, 155), gfx::Size(50, 50)),
            drag_icon_proxy->GetBoundsInScreen());
  EXPECT_EQ(target_bounds,
            GetTargetAppDragIconImageBounds(drag_icon_proxy.get()));

  // Wait for the layer animation to complete.
  run_loop.Run();

  // The widget should be closed at this point - verify public methods do no
  // crash if used.
  EXPECT_EQ(gfx::Rect(), drag_icon_proxy->GetBoundsInScreen());
  drag_icon_proxy->UpdatePosition(gfx::Point(20, 20));
  EXPECT_EQ(gfx::Rect(), drag_icon_proxy->GetBoundsInScreen());
  drag_icon_proxy->SetOpacity(1.0f);
}

// Verifies that "close" animation completion callback gets called if the proxy
// is reset during the animation.
TEST_F(AppDragIconProxyTest, ProxyResetDuringCloseAnimation) {
  auto drag_icon_proxy = std::make_unique<AppDragIconProxy>(
      Shell::GetPrimaryRootWindow(),
      ash::image_util::CreateEmptyImage(gfx::Size(50, 50)),
      /*pointer_location_in_screen=*/gfx::Point(100, 200),
      /*pointer_offset_from_center=*/gfx::Vector2d(10, 20),
      /*scale_factor=*/1.0f,
      /*use_blurred_background=*/false);
  EXPECT_EQ(gfx::Rect(gfx::Point(65, 155), gfx::Size(50, 50)),
            drag_icon_proxy->GetBoundsInScreen());

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  bool animation_callback_called = false;
  gfx::Rect target_bounds = gfx::Rect(gfx::Point(50, 50), gfx::Size(10, 10));
  drag_icon_proxy->AnimateToBoundsAndCloseWidget(
      target_bounds, base::BindLambdaForTesting([&animation_callback_called]() {
        animation_callback_called = true;
      }));
  EXPECT_FALSE(animation_callback_called);

  drag_icon_proxy.reset();
  EXPECT_TRUE(animation_callback_called);
}

// Tests that calls to `UpdatePosition()` are ignored if
// `AnimateToBoundsAndCloseWidget()` has been called.
TEST_F(AppDragIconProxyTest, UpdatePositionDuringCloseIsNoOp) {
  auto drag_icon_proxy = std::make_unique<AppDragIconProxy>(
      Shell::GetPrimaryRootWindow(),
      ash::image_util::CreateEmptyImage(gfx::Size(50, 50)),
      /*pointer_location_in_screen=*/gfx::Point(100, 200),
      /*pointer_offset_from_center=*/gfx::Vector2d(10, 20),
      /*scale_factor=*/1.0f,
      /*use_blurred_background=*/false);
  EXPECT_EQ(gfx::Rect(gfx::Point(65, 155), gfx::Size(50, 50)),
            drag_icon_proxy->GetBoundsInScreen());

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  base::RunLoop run_loop;
  gfx::Rect target_bounds = gfx::Rect(gfx::Point(50, 50), gfx::Size(10, 10));
  drag_icon_proxy->AnimateToBoundsAndCloseWidget(target_bounds,
                                                 run_loop.QuitClosure());

  EXPECT_EQ(gfx::Rect(gfx::Point(65, 155), gfx::Size(50, 50)),
            drag_icon_proxy->GetBoundsInScreen());
  EXPECT_EQ(target_bounds,
            GetTargetAppDragIconImageBounds(drag_icon_proxy.get()));

  // Updating location should not work at this point.
  drag_icon_proxy->UpdatePosition(gfx::Point(200, 300));
  EXPECT_EQ(gfx::Rect(gfx::Point(65, 155), gfx::Size(50, 50)),
            drag_icon_proxy->GetBoundsInScreen());
  EXPECT_EQ(target_bounds,
            GetTargetAppDragIconImageBounds(drag_icon_proxy.get()));

  // Verify the animation callback gets called.
  run_loop.Run();

  // The widget should be closed at this point - verify public methods do no
  // crash if used.
  EXPECT_EQ(gfx::Rect(), drag_icon_proxy->GetBoundsInScreen());
  drag_icon_proxy->UpdatePosition(gfx::Point(20, 20));
  EXPECT_EQ(gfx::Rect(), drag_icon_proxy->GetBoundsInScreen());
  drag_icon_proxy->SetOpacity(1.0f);
}

}  // namespace ash
