// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/colors/assistant_colors_util.h"

#include "ash/assistant/ui/colors/assistant_colors.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {
namespace assistant {

using AssistantColorsUtilUnittest = AshTestBase;

TEST_F(AssistantColorsUtilUnittest, AssistantColor) {
  base::test::ScopedFeatureList scoped_feature_list(features::kDarkLightMode);
  AshColorProvider::Get()->OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetActivePrefService());

  EXPECT_EQ(
      ResolveAssistantColor(assistant_colors::ColorName::kBgAssistantPlate),
      assistant_colors::ResolveColor(
          assistant_colors::ColorName::kBgAssistantPlate,
          /*is_dark_mode=*/false, /*use_debug_colors=*/false));

  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      prefs::kDarkModeEnabled, true);

  EXPECT_EQ(
      ResolveAssistantColor(assistant_colors::ColorName::kBgAssistantPlate),
      assistant_colors::ResolveColor(
          assistant_colors::ColorName::kBgAssistantPlate,
          /*is_dark_mode=*/true, /*use_debug_colors=*/false));
}

TEST_F(AssistantColorsUtilUnittest, AssistantColorFlagOff) {
  ASSERT_FALSE(features::IsDarkLightModeEnabled());

  EXPECT_EQ(
      ResolveAssistantColor(assistant_colors::ColorName::kBgAssistantPlate),
      SK_ColorWHITE);
  EXPECT_EQ(
      ResolveAssistantColor(assistant_colors::ColorName::kBgAssistantPlate),
      SK_ColorWHITE);
}

// ResolveAssistantColor falls back to assistant_colors::ResolveColor with dark
// mode off if the color is not defined in the cc file map and the flag is off.
TEST_F(AssistantColorsUtilUnittest, AssistantColorFlagOffFallback) {
  ASSERT_FALSE(features::IsDarkLightModeEnabled());

  EXPECT_EQ(ResolveAssistantColor(assistant_colors::ColorName::kGoogleBlue100),
            assistant_colors::ResolveColor(
                assistant_colors::ColorName::kGoogleBlue100,
                /*is_dark_mode=*/false, /*use_debug_colors=*/false));
}

}  // namespace assistant
}  // namespace ash
