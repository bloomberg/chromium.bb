// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_text_element_view.h"

#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr char kTestString[] = "test";

using AssistantTextElementViewTest = AshTestBase;

TEST_F(AssistantTextElementViewTest, DarkAndLightTheme) {
  base::test::ScopedFeatureList scoped_feature_list(
      chromeos::features::kDarkLightMode);
  ASSERT_TRUE(chromeos::features::IsDarkLightModeEnabled());

  auto* color_provider = AshColorProvider::Get();
  color_provider->OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetActivePrefService());
  const bool initial_dark_mode_status = color_provider->IsDarkModeEnabled();

  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();
  AssistantTextElementView* text_element_view = widget->SetContentsView(
      std::make_unique<AssistantTextElementView>(kTestString));

  views::Label* label =
      static_cast<views::Label*>(text_element_view->children().at(0));

  EXPECT_EQ(label->GetEnabledColor(),
            color_provider->GetContentLayerColor(
                ColorProvider::ContentLayerType::kTextColorPrimary));

  // Switch the color mode.
  color_provider->ToggleColorMode();
  ASSERT_NE(initial_dark_mode_status, color_provider->IsDarkModeEnabled());

  EXPECT_EQ(label->GetEnabledColor(),
            color_provider->GetContentLayerColor(
                ColorProvider::ContentLayerType::kTextColorPrimary));
}

TEST_F(AssistantTextElementViewTest, DarkAndLightModeFlagOff) {
  ASSERT_FALSE(chromeos::features::IsDarkLightModeEnabled());

  // ProductivityLauncher uses DarkLightMode colors.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kProductivityLauncher);

  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();
  AssistantTextElementView* text_element_view = widget->SetContentsView(
      std::make_unique<AssistantTextElementView>(kTestString));

  views::Label* label =
      static_cast<views::Label*>(text_element_view->children().at(0));
  EXPECT_EQ(label->GetEnabledColor(), kTextColorPrimary);
}

}  // namespace
}  // namespace ash
