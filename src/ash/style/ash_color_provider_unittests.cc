// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/ash_color_provider.h"

#include "ash/constants/ash_features.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

using AshColorProviderTest = AshTestBase;

// Tests the color mode in non-active user sessions.
TEST_F(AshColorProviderTest, ColorModeInNonActiveUserSessions) {
  auto* client = GetSessionControllerClient();
  auto* color_provider = AshColorProvider::Get();

  // When dark/light mode is enabled. Color mode in non-active user sessions
  // (e.g, login page) should be DARK, but LIGHT while in OOBE.
  base::test::ScopedFeatureList enable_dark_light;
  enable_dark_light.InitAndEnableFeature(features::kDarkLightMode);
  client->SetSessionState(session_manager::SessionState::UNKNOWN);
  EXPECT_TRUE(color_provider->IsDarkModeEnabled());
  client->SetSessionState(session_manager::SessionState::OOBE);
  EXPECT_FALSE(color_provider->IsDarkModeEnabled());

  // When dark/light mode is disabled. Color mode in non-active user sessions
  // (e.g, login page) should still be DARK.
  base::test::ScopedFeatureList disable_dark_light;
  disable_dark_light.InitAndDisableFeature(features::kDarkLightMode);
  client->SetSessionState(session_manager::SessionState::UNKNOWN);
  EXPECT_TRUE(color_provider->IsDarkModeEnabled());
  client->SetSessionState(session_manager::SessionState::OOBE);
  EXPECT_TRUE(color_provider->IsDarkModeEnabled());
}

}  // namespace ash
