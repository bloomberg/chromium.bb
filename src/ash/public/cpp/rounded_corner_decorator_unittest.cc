// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/rounded_corner_decorator.h"

#include "ash/public/cpp/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"

namespace ash {

class RoundedCornerDecoratorTest : public aura::test::AuraTestBase,
                                   public testing::WithParamInterface<bool> {
 public:
  RoundedCornerDecoratorTest() = default;
  ~RoundedCornerDecoratorTest() override = default;

  void SetUp() override {
    aura::test::AuraTestBase::SetUp();
    if (UseShaderForRoundedCorner()) {
      scoped_feature_list_.InitAndEnableFeature(
          ash::features::kUseShaderRoundedCorner);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          ash::features::kUseShaderRoundedCorner);
    }
  }

  bool UseShaderForRoundedCorner() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(RoundedCornerDecoratorTest);
};

// Test that the decorator doesn't try to apply itself to destroyed layers.
TEST_P(RoundedCornerDecoratorTest, RoundedCornerMaskProperlyInvalidatesItself) {
  constexpr int kCornerRadius = 4;
  constexpr gfx::RoundedCornersF kRadii(kCornerRadius);
  std::unique_ptr<aura::Window> window(aura::test::CreateTestWindowWithBounds(
      gfx::Rect(100, 100, 100, 100), root_window()));
  auto decorator = std::make_unique<ash::RoundedCornerDecorator>(
      window.get(), window.get(), window->layer(), kCornerRadius);

  // Confirm a mask layer exists and the decorator is valid.
  EXPECT_TRUE(window->layer());
  if (UseShaderForRoundedCorner()) {
    ASSERT_FALSE(window->layer()->layer_mask_layer());
    EXPECT_EQ(window->layer()->rounded_corner_radii(), kRadii);
  } else {
    EXPECT_TRUE(window->layer()->layer_mask_layer());
  }
  EXPECT_TRUE(decorator->IsValid());

  // Destroy window.
  window.reset();

  // Existing layer was destroyed, so the decorator should no longer be valid.
  EXPECT_FALSE(decorator->IsValid());
}

// Test that mask layer changes bounds with the window it is applied to.
TEST_P(RoundedCornerDecoratorTest,
       RoundedCornerMaskChangesBoundsOnWindowBoundsChange) {
  constexpr int kCornerRadius = 4;
  constexpr gfx::RoundedCornersF kRadii(kCornerRadius);
  std::unique_ptr<aura::Window> window(aura::test::CreateTestWindowWithBounds(
      gfx::Rect(100, 100, 100, 100), root_window()));
  auto decorator = std::make_unique<ash::RoundedCornerDecorator>(
      window.get(), window.get(), window->layer(), kCornerRadius);

  if (UseShaderForRoundedCorner()) {
    ASSERT_FALSE(window->layer()->layer_mask_layer());
    EXPECT_EQ(window->layer()->rounded_corner_radii(), kRadii);
  } else {
    // Make sure the mask layer has the correct bounds and exists.
    ASSERT_TRUE(window->layer()->layer_mask_layer());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 100),
              window->layer()->layer_mask_layer()->bounds());
  }

  // Change the bounds of the window. Set zero duration animations to apply
  // changes immediately.
  window->SetBounds(gfx::Rect(0, 0, 150, 150));

  // Make sure the mask layer's bounds are also changed.
  EXPECT_EQ(gfx::Rect(0, 0, 150, 150).ToString(), window->bounds().ToString());

  if (UseShaderForRoundedCorner()) {
    ASSERT_FALSE(window->layer()->layer_mask_layer());
    EXPECT_EQ(window->layer()->rounded_corner_radii(), kRadii);
  } else {
    EXPECT_EQ(window->layer()->layer_mask_layer()->bounds().ToString(),
              window->bounds().ToString());
  }
}

INSTANTIATE_TEST_SUITE_P(
    /* prefix intentionally left blank due to only one parameterization */,
    RoundedCornerDecoratorTest,
    testing::Bool());

}  // namespace ash
