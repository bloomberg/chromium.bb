// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/ash_color_provider.h"

#include "ash/constants/ash_features.h"
#include "ash/login/login_screen_controller.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/public/cpp/login_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"

namespace ash {

using AshColorProviderTest = NoSessionAshTestBase;

// Tests the color mode in non-active user sessions.
TEST_F(AshColorProviderTest, ColorModeInNonActiveUserSessions) {
  auto* client = GetSessionControllerClient();
  auto* color_provider = AshColorProvider::Get();

  base::test::ScopedFeatureList enable_dark_light;
  enable_dark_light.InitAndEnableFeature(chromeos::features::kDarkLightMode);
  client->SetSessionState(session_manager::SessionState::UNKNOWN);
  // When dark/light mode is enabled. Color mode in non-active user sessions
  // (e.g, login page) should be DARK.
  auto* active_user_pref_service =
      Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  ASSERT_FALSE(active_user_pref_service);
  EXPECT_TRUE(color_provider->IsDarkModeEnabled());

  // But color mode should be LIGHT in OOBE.
  auto* dispatcher = Shell::Get()->login_screen_controller()->data_dispatcher();
  client->SetSessionState(session_manager::SessionState::OOBE);
  dispatcher->NotifyOobeDialogState(OobeDialogState::USER_CREATION);
  EXPECT_FALSE(color_provider->IsDarkModeEnabled());

  client->SetSessionState(session_manager::SessionState::LOGIN_PRIMARY);
  dispatcher->NotifyOobeDialogState(OobeDialogState::HIDDEN);
  EXPECT_TRUE(color_provider->IsDarkModeEnabled());

  dispatcher->NotifyOobeDialogState(OobeDialogState::GAIA_SIGNIN);
  EXPECT_FALSE(color_provider->IsDarkModeEnabled());

  // When dark/light mode is disabled. Color mode in non-active user sessions
  // (e.g, login page) should still be DARK.
  base::test::ScopedFeatureList disable_dark_light;
  disable_dark_light.InitWithFeatures(
      /*enabled_features=*/{}, /*disabled_features=*/{
          chromeos::features::kDarkLightMode, features::kNotificationsRefresh});
  client->SetSessionState(session_manager::SessionState::UNKNOWN);
  EXPECT_TRUE(color_provider->IsDarkModeEnabled());
  client->SetSessionState(session_manager::SessionState::OOBE);
  dispatcher->NotifyOobeDialogState(OobeDialogState::USER_CREATION);
  EXPECT_TRUE(color_provider->IsDarkModeEnabled());
}

}  // namespace ash
