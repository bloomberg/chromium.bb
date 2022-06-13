// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_onboarding_suggestion_view.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/services/libassistant/public/cpp/assistant_suggestion.h"
#include "components/prefs/pref_service.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

AssistantOnboardingSuggestionView* CreateSuggestionViewAt(
    int index,
    views::Widget* widget) {
  chromeos::assistant::AssistantSuggestion assistant_suggestion;
  return widget->GetContentsView()->AddChildView(
      std::make_unique<AssistantOnboardingSuggestionView>(
          /*delegate=*/nullptr, assistant_suggestion, index));
}

views::Label* GetLabel(AssistantOnboardingSuggestionView* suggestion_view) {
  return static_cast<views::Label*>(
      suggestion_view->GetViewByID(kAssistantOnboardingSuggestionViewLabel));
}

}  // namespace

using AssistantOnboardingSuggestionViewTest = AshTestBase;

TEST_F(AssistantOnboardingSuggestionViewTest, DarkAndLightTheme) {
  base::test::ScopedFeatureList scoped_feature_list(features::kDarkLightMode);
  AshColorProvider::Get()->OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetActivePrefService());
  ASSERT_TRUE(features::IsDarkLightModeEnabled());
  ASSERT_FALSE(ColorProvider::Get()->IsDarkModeEnabled());

  std::unique_ptr<views::Widget> widget = CreateTestWidget();

  AssistantOnboardingSuggestionView* suggestion_view_0 =
      CreateSuggestionViewAt(0, widget.get());
  AssistantOnboardingSuggestionView* suggestion_view_1 =
      CreateSuggestionViewAt(1, widget.get());
  AssistantOnboardingSuggestionView* suggestion_view_2 =
      CreateSuggestionViewAt(2, widget.get());
  AssistantOnboardingSuggestionView* suggestion_view_3 =
      CreateSuggestionViewAt(3, widget.get());
  AssistantOnboardingSuggestionView* suggestion_view_4 =
      CreateSuggestionViewAt(4, widget.get());
  AssistantOnboardingSuggestionView* suggestion_view_5 =
      CreateSuggestionViewAt(5, widget.get());

  // 0x19 is for 10% alpha. 255*0.1=25.5. -> 25 in hex is 0x19.
  EXPECT_EQ(suggestion_view_0->GetBackground()->get_color(),
            SkColorSetA(gfx::kGoogleBlue600, 0x19));
  EXPECT_EQ(GetLabel(suggestion_view_0)->GetEnabledColor(),
            gfx::kGoogleBlue800);

  EXPECT_EQ(suggestion_view_1->GetBackground()->get_color(),
            SkColorSetA(gfx::kGoogleRed600, 0x19));
  EXPECT_EQ(GetLabel(suggestion_view_1)->GetEnabledColor(), gfx::kGoogleRed800);

  EXPECT_EQ(suggestion_view_2->GetBackground()->get_color(),
            SkColorSetA(gfx::kGoogleYellow600, 0x19));
  EXPECT_EQ(GetLabel(suggestion_view_2)->GetEnabledColor(),
            SkColorSetRGB(0xBF, 0x50, 0x00));

  EXPECT_EQ(suggestion_view_3->GetBackground()->get_color(),
            SkColorSetA(gfx::kGoogleGreen600, 0x19));
  EXPECT_EQ(GetLabel(suggestion_view_3)->GetEnabledColor(),
            gfx::kGoogleGreen800);

  EXPECT_EQ(suggestion_view_4->GetBackground()->get_color(),
            SkColorSetARGB(0x19, 0xc6, 0x1a, 0xd9));
  EXPECT_EQ(GetLabel(suggestion_view_4)->GetEnabledColor(),
            SkColorSetRGB(0xaa, 0x00, 0xb8));

  EXPECT_EQ(suggestion_view_5->GetBackground()->get_color(),
            SkColorSetA(gfx::kGoogleBlue600, 0x19));
  EXPECT_EQ(GetLabel(suggestion_view_5)->GetEnabledColor(),
            gfx::kGoogleBlue800);

  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      prefs::kDarkModeEnabled, true);
  ASSERT_TRUE(ColorProvider::Get()->IsDarkModeEnabled());

  // 0x4c is for 30% alpha. 255*0.3=76.5. 0x4c is 76 in hex.
  EXPECT_EQ(suggestion_view_0->GetBackground()->get_color(),
            SkColorSetA(gfx::kGoogleBlue300, 0x4c));
  EXPECT_EQ(GetLabel(suggestion_view_0)->GetEnabledColor(),
            gfx::kGoogleBlue200);

  EXPECT_EQ(suggestion_view_1->GetBackground()->get_color(),
            SkColorSetA(gfx::kGoogleRed300, 0x4c));
  EXPECT_EQ(GetLabel(suggestion_view_1)->GetEnabledColor(), gfx::kGoogleRed200);

  EXPECT_EQ(suggestion_view_2->GetBackground()->get_color(),
            SkColorSetA(gfx::kGoogleYellow300, 0x4c));
  EXPECT_EQ(GetLabel(suggestion_view_2)->GetEnabledColor(),
            gfx::kGoogleYellow200);

  EXPECT_EQ(suggestion_view_3->GetBackground()->get_color(),
            SkColorSetA(gfx::kGoogleGreen300, 0x4c));
  EXPECT_EQ(GetLabel(suggestion_view_3)->GetEnabledColor(),
            gfx::kGoogleGreen200);

  EXPECT_EQ(suggestion_view_4->GetBackground()->get_color(),
            SkColorSetARGB(0x4c, 0xf8, 0x82, 0xff));
  EXPECT_EQ(GetLabel(suggestion_view_4)->GetEnabledColor(),
            SkColorSetRGB(0xf8, 0x82, 0xff));

  EXPECT_EQ(suggestion_view_5->GetBackground()->get_color(),
            SkColorSetA(gfx::kGoogleBlue300, 0x4c));
  EXPECT_EQ(GetLabel(suggestion_view_5)->GetEnabledColor(),
            gfx::kGoogleBlue200);
}

TEST_F(AssistantOnboardingSuggestionViewTest, DarkAndLightModeFlagOff) {
  ASSERT_FALSE(features::IsDarkLightModeEnabled());

  std::unique_ptr<views::Widget> widget = CreateTestWidget();

  AssistantOnboardingSuggestionView* suggestion_view_0 =
      CreateSuggestionViewAt(0, widget.get());
  AssistantOnboardingSuggestionView* suggestion_view_1 =
      CreateSuggestionViewAt(1, widget.get());
  AssistantOnboardingSuggestionView* suggestion_view_2 =
      CreateSuggestionViewAt(2, widget.get());
  AssistantOnboardingSuggestionView* suggestion_view_3 =
      CreateSuggestionViewAt(3, widget.get());
  AssistantOnboardingSuggestionView* suggestion_view_4 =
      CreateSuggestionViewAt(4, widget.get());
  AssistantOnboardingSuggestionView* suggestion_view_5 =
      CreateSuggestionViewAt(5, widget.get());

  EXPECT_EQ(suggestion_view_0->GetBackground()->get_color(),
            gfx::kGoogleBlue050);
  EXPECT_EQ(GetLabel(suggestion_view_0)->GetEnabledColor(),
            gfx::kGoogleBlue800);

  EXPECT_EQ(suggestion_view_1->GetBackground()->get_color(),
            gfx::kGoogleRed050);
  EXPECT_EQ(GetLabel(suggestion_view_1)->GetEnabledColor(), gfx::kGoogleRed800);

  EXPECT_EQ(suggestion_view_2->GetBackground()->get_color(),
            gfx::kGoogleYellow050);
  EXPECT_EQ(GetLabel(suggestion_view_2)->GetEnabledColor(),
            SkColorSetRGB(0xBF, 0x50, 0x00));

  EXPECT_EQ(suggestion_view_3->GetBackground()->get_color(),
            gfx::kGoogleGreen050);
  EXPECT_EQ(GetLabel(suggestion_view_3)->GetEnabledColor(),
            gfx::kGoogleGreen800);

  EXPECT_EQ(suggestion_view_4->GetBackground()->get_color(),
            SkColorSetRGB(0xF6, 0xE9, 0xF8));
  EXPECT_EQ(GetLabel(suggestion_view_4)->GetEnabledColor(),
            SkColorSetRGB(0x8A, 0x0E, 0x9E));

  EXPECT_EQ(suggestion_view_5->GetBackground()->get_color(),
            gfx::kGoogleBlue050);
  EXPECT_EQ(GetLabel(suggestion_view_5)->GetEnabledColor(),
            gfx::kGoogleBlue800);
}

}  // namespace ash
