// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/assistant/assistant_dialog_plate.h"

#include "ash/app_list/views/app_list_view.h"
#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/base/assistant_button.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {
namespace {

constexpr int kIconDipSize = 24;

}  // namespace

using AssistantDialogPlateTest = AssistantAshTestBase;

TEST_F(AssistantDialogPlateTest, DarkAndLightTheme) {
  base::test::ScopedFeatureList scoped_feature_list(
      chromeos::features::kDarkLightMode);
  AshColorProvider::Get()->OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetActivePrefService());
  ASSERT_TRUE(chromeos::features::IsDarkLightModeEnabled());
  ASSERT_FALSE(ColorProvider::Get()->IsDarkModeEnabled());

  ShowAssistantUi();

  views::View* assistant_dialog_plate =
      page_view()->GetViewByID(AssistantViewID::kDialogPlate);
  views::Textfield* assistant_text_field = static_cast<views::Textfield*>(
      assistant_dialog_plate->GetViewByID(AssistantViewID::kTextQueryField));
  AssistantButton* keyboard_input_toggle =
      static_cast<AssistantButton*>(assistant_dialog_plate->GetViewByID(
          AssistantViewID::kKeyboardInputToggle));

  EXPECT_EQ(assistant_text_field->GetTextColor(),
            ColorProvider::Get()->GetContentLayerColor(
                ColorProvider::ContentLayerType::kTextColorPrimary));
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      *gfx::CreateVectorIcon(kKeyboardIcon, kIconDipSize, gfx::kGoogleGrey900)
           .bitmap(),
      *keyboard_input_toggle->GetImage(views::Button::STATE_NORMAL).bitmap()));

  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      prefs::kDarkModeEnabled, true);
  ASSERT_TRUE(ColorProvider::Get()->IsDarkModeEnabled());

  EXPECT_EQ(assistant_text_field->GetTextColor(),
            ColorProvider::Get()->GetContentLayerColor(
                ColorProvider::ContentLayerType::kTextColorPrimary));
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      *gfx::CreateVectorIcon(kKeyboardIcon, kIconDipSize, gfx::kGoogleGrey200)
           .bitmap(),
      *keyboard_input_toggle->GetImage(views::Button::STATE_NORMAL).bitmap()));
}

TEST_F(AssistantDialogPlateTest, DarkAndLightModeFlagOff) {
  ASSERT_FALSE(chromeos::features::IsDarkLightModeEnabled());

  // ProductivityLauncher uses DarkLightMode colors.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kProductivityLauncher);

  ShowAssistantUi();

  views::View* assistant_dialog_plate =
      page_view()->GetViewByID(AssistantViewID::kDialogPlate);
  views::Textfield* assistant_text_field = static_cast<views::Textfield*>(
      assistant_dialog_plate->GetViewByID(AssistantViewID::kTextQueryField));
  AssistantButton* keyboard_input_toggle =
      static_cast<AssistantButton*>(assistant_dialog_plate->GetViewByID(
          AssistantViewID::kKeyboardInputToggle));

  EXPECT_EQ(assistant_text_field->GetTextColor(), kTextColorPrimary);
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      *gfx::CreateVectorIcon(kKeyboardIcon, kIconDipSize, gfx::kGoogleGrey900)
           .bitmap(),
      *keyboard_input_toggle->GetImage(views::Button::STATE_NORMAL).bitmap()));

  // Avoid test teardown issues by explicitly closing the launcher.
  CloseAssistantUi();
}

}  // namespace ash
