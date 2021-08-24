// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_zero_state_view.h"

#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/colors/assistant_colors.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "base/test/scoped_feature_list.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/chromeos/colors/cros_colors.h"
#include "ui/views/controls/label.h"

namespace ash {
namespace {

using AssistantZeroStateViewUnittest = AssistantAshTestBase;

TEST_F(AssistantZeroStateViewUnittest, Theme) {
  ASSERT_FALSE(features::IsDarkLightModeEnabled());

  ShowAssistantUi();

  const views::Label* greeting_label = static_cast<views::Label*>(
      main_view()->GetViewByID(AssistantViewID::kGreetingLabel));

  EXPECT_EQ(greeting_label->GetBackgroundColor(), SK_ColorWHITE);
  EXPECT_EQ(greeting_label->GetEnabledColor(), kTextColorPrimary);
}

TEST_F(AssistantZeroStateViewUnittest, ThemeDarkLightMode) {
  base::test::ScopedFeatureList scoped_feature_list(features::kDarkLightMode);
  AshColorProvider::Get()->OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetActivePrefService());

  ShowAssistantUi();

  const views::Label* greeting_label = static_cast<views::Label*>(
      main_view()->GetViewByID(AssistantViewID::kGreetingLabel));

  EXPECT_EQ(greeting_label->GetBackgroundColor(),
            assistant_colors::ResolveColor(
                assistant_colors::ColorName::kBgAssistantPlate,
                /*is_dark_mode=*/false, /*use_debug_colors=*/false));
  EXPECT_EQ(greeting_label->GetEnabledColor(),
            cros_colors::ResolveColor(cros_colors::ColorName::kTextColorPrimary,
                                      /*is_dark_mode=*/false,
                                      /*use_debug_colors=*/false));

  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      prefs::kDarkModeEnabled, true);

  EXPECT_EQ(greeting_label->GetBackgroundColor(),
            assistant_colors::ResolveColor(
                assistant_colors::ColorName::kBgAssistantPlate,
                /*is_dark_mode=*/true, /*use_debug_colors=*/false));
  EXPECT_EQ(greeting_label->GetEnabledColor(),
            cros_colors::ResolveColor(cros_colors::ColorName::kTextColorPrimary,
                                      /*is_dark_mode=*/true,
                                      /*use_debug_colors=*/false));
}

}  // namespace
}  // namespace ash
