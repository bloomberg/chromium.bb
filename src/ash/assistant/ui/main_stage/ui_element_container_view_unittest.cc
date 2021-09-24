// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/ui_element_container_view.h"

#include "ash/assistant/assistant_interaction_controller_impl.h"
#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "base/test/scoped_feature_list.h"
#include "cc/base/math_util.h"
#include "chromeos/services/libassistant/public/cpp/assistant_interaction_metadata.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/background.h"
#include "ui/views/view.h"

namespace ash {

namespace {
constexpr char kResponseText[] = "Response";
}

// Use AssistantAshTestBase as we expect that UiElementContainer::OnThemeChanged
// gets called with dark and light mode preference change.
using UiElementContainerViewTest = AssistantAshTestBase;

TEST_F(UiElementContainerViewTest, DarkAndLightTheme) {
  base::test::ScopedFeatureList scoped_feature_list(features::kDarkLightMode);
  AshColorProvider::Get()->OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetActivePrefService());
  ASSERT_TRUE(features::IsDarkLightModeEnabled());
  ASSERT_FALSE(AshColorProvider::Get()->IsDarkModeEnabled());

  ShowAssistantUi();

  views::View* ui_element_container_view =
      page_view()->GetViewByID(kUiElementContainer);
  views::View* indicator =
      ui_element_container_view->GetViewByID(kOverflowIndicator);
  EXPECT_EQ(indicator->GetBackground()->get_color(),
            AshColorProvider::Get()->GetContentLayerColor(
                ColorProvider::ContentLayerType::kSeparatorColor));

  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      prefs::kDarkModeEnabled, true);
  ASSERT_TRUE(AshColorProvider::Get()->IsDarkModeEnabled());

  EXPECT_EQ(indicator->GetBackground()->get_color(),
            AshColorProvider::Get()->GetContentLayerColor(
                ColorProvider::ContentLayerType::kSeparatorColor));
}

TEST_F(UiElementContainerViewTest, DarkAndLightModeFlagOff) {
  ASSERT_FALSE(features::IsDarkLightModeEnabled());

  ShowAssistantUi();

  views::View* ui_element_container_view =
      page_view()->GetViewByID(kUiElementContainer);
  views::View* indicator =
      ui_element_container_view->GetViewByID(kOverflowIndicator);
  EXPECT_EQ(indicator->GetBackground()->get_color(), gfx::kGoogleGrey300);
}

TEST_F(UiElementContainerViewTest, CustomOverflowIndicator) {
  ShowAssistantUi();

  UiElementContainerView* ui_element_container_view =
      static_cast<UiElementContainerView*>(
          page_view()->GetViewByID(kUiElementContainer));
  views::View* indicator =
      ui_element_container_view->GetViewByID(kOverflowIndicator);

  AssistantInteractionControllerImpl* controller =
      static_cast<AssistantInteractionControllerImpl*>(
          AssistantInteractionController::Get());
  controller->OnInteractionStarted(
      chromeos::assistant::AssistantInteractionMetadata());

  // Add a single text response and confirm that overflow indicator is not
  // visible.
  controller->OnTextResponse(kResponseText);

  ASSERT_LT(ui_element_container_view->content_view()->height(),
            ui_element_container_view->height());
  EXPECT_TRUE(cc::MathUtil::IsWithinEpsilon(
      0.0f, indicator->layer()->GetTargetOpacity()));

  // Add 20 text responses as scroll becomes necessary.
  for (int i = 0; i < 20; ++i) {
    controller->OnTextResponse(kResponseText);
  }

  ASSERT_GT(ui_element_container_view->content_view()->height(),
            ui_element_container_view->height());
  EXPECT_TRUE(cc::MathUtil::IsWithinEpsilon(
      1.0f, indicator->layer()->GetTargetOpacity()));
}

}  // namespace ash
