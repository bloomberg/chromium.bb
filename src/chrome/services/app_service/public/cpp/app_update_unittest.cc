// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/app_service/public/cpp/app_update.h"
#include "testing/gtest/include/gtest/gtest.h"

class AppUpdateTest : public testing::Test {};

TEST_F(AppUpdateTest, BasicOps) {
  const apps::mojom::AppType app_type = apps::mojom::AppType::kArc;
  const std::string app_id = "abcdefgh";

  apps::mojom::AppPtr state = apps::mojom::App::New();
  state->app_type = app_type;
  state->app_id = app_id;

  apps::mojom::AppPtr delta = apps::mojom::App::New();
  delta->app_type = app_type;
  delta->app_id = app_id;

  apps::AppUpdate u(state, delta);

  EXPECT_EQ(app_type, u.AppType());
  EXPECT_EQ(app_id, u.AppId());

  EXPECT_FALSE(u.ReadinessChanged());
  EXPECT_FALSE(u.NameChanged());
  EXPECT_FALSE(u.ShowInLauncherChanged());

  EXPECT_EQ(apps::mojom::Readiness::kUnknown, u.Readiness());
  EXPECT_EQ("", u.Name());
  EXPECT_EQ(apps::mojom::OptionalBool::kUnknown, u.ShowInLauncher());

  delta->name = "Inigo Montoya";

  EXPECT_FALSE(u.ReadinessChanged());
  EXPECT_TRUE(u.NameChanged());
  EXPECT_FALSE(u.ShowInLauncherChanged());

  EXPECT_EQ(apps::mojom::Readiness::kUnknown, u.Readiness());
  EXPECT_NE(apps::mojom::Readiness::kReady, state->readiness);
  EXPECT_EQ("Inigo Montoya", u.Name());
  EXPECT_NE("Inigo Montoya", state->name);
  EXPECT_EQ(apps::mojom::OptionalBool::kUnknown, u.ShowInLauncher());
  EXPECT_EQ(apps::mojom::OptionalBool::kUnknown, state->show_in_launcher);

  state->name = "Inigo Montoya";

  EXPECT_FALSE(u.ReadinessChanged());
  EXPECT_FALSE(u.NameChanged());
  EXPECT_FALSE(u.ShowInLauncherChanged());

  EXPECT_EQ(apps::mojom::Readiness::kUnknown, u.Readiness());
  EXPECT_NE(apps::mojom::Readiness::kReady, state->readiness);
  EXPECT_EQ("Inigo Montoya", u.Name());
  EXPECT_EQ("Inigo Montoya", state->name);
  EXPECT_EQ(apps::mojom::OptionalBool::kUnknown, u.ShowInLauncher());
  EXPECT_EQ(apps::mojom::OptionalBool::kUnknown, state->show_in_launcher);

  delta->readiness = apps::mojom::Readiness::kReady;

  EXPECT_TRUE(u.ReadinessChanged());
  EXPECT_FALSE(u.NameChanged());
  EXPECT_FALSE(u.ShowInLauncherChanged());

  EXPECT_EQ(apps::mojom::Readiness::kReady, u.Readiness());
  EXPECT_NE(apps::mojom::Readiness::kReady, state->readiness);
  EXPECT_EQ("Inigo Montoya", u.Name());
  EXPECT_EQ("Inigo Montoya", state->name);
  EXPECT_EQ(apps::mojom::OptionalBool::kUnknown, u.ShowInLauncher());
  EXPECT_EQ(apps::mojom::OptionalBool::kUnknown, state->show_in_launcher);

  delta->name = base::nullopt;

  EXPECT_TRUE(u.ReadinessChanged());
  EXPECT_FALSE(u.NameChanged());
  EXPECT_FALSE(u.ShowInLauncherChanged());

  EXPECT_EQ(apps::mojom::Readiness::kReady, u.Readiness());
  EXPECT_NE(apps::mojom::Readiness::kReady, state->readiness);
  EXPECT_EQ("Inigo Montoya", u.Name());
  EXPECT_EQ("Inigo Montoya", state->name);
  EXPECT_EQ(apps::mojom::OptionalBool::kUnknown, u.ShowInLauncher());
  EXPECT_EQ(apps::mojom::OptionalBool::kUnknown, state->show_in_launcher);

  apps::AppUpdate::Merge(state.get(), delta);

  EXPECT_FALSE(u.ReadinessChanged());
  EXPECT_FALSE(u.NameChanged());
  EXPECT_FALSE(u.ShowInLauncherChanged());

  EXPECT_EQ(apps::mojom::Readiness::kReady, u.Readiness());
  EXPECT_EQ(apps::mojom::Readiness::kReady, state->readiness);
  EXPECT_EQ("Inigo Montoya", u.Name());
  EXPECT_EQ("Inigo Montoya", state->name);
  EXPECT_EQ(apps::mojom::OptionalBool::kUnknown, u.ShowInLauncher());
  EXPECT_EQ(apps::mojom::OptionalBool::kUnknown, state->show_in_launcher);

  delta->readiness = apps::mojom::Readiness::kDisabledByPolicy;
  delta->name = "Dread Pirate Roberts";

  EXPECT_TRUE(u.ReadinessChanged());
  EXPECT_TRUE(u.NameChanged());
  EXPECT_FALSE(u.ShowInLauncherChanged());

  EXPECT_NE(apps::mojom::Readiness::kReady, u.Readiness());
  EXPECT_EQ(apps::mojom::Readiness::kReady, state->readiness);
  EXPECT_NE("Inigo Montoya", u.Name());
  EXPECT_EQ("Inigo Montoya", state->name);
  EXPECT_EQ(apps::mojom::OptionalBool::kUnknown, u.ShowInLauncher());
  EXPECT_EQ(apps::mojom::OptionalBool::kUnknown, state->show_in_launcher);

  // IconKey tests.

  EXPECT_TRUE(u.IconKey().is_null());
  EXPECT_FALSE(u.IconKeyChanged());

  state->icon_key = apps::mojom::IconKey::New(apps::mojom::IconType::kResource,
                                              100, std::string());

  EXPECT_FALSE(u.IconKey().is_null());
  EXPECT_EQ(apps::mojom::IconType::kResource, u.IconKey()->icon_type);
  EXPECT_FALSE(u.IconKeyChanged());

  delta->icon_key = apps::mojom::IconKey::New(apps::mojom::IconType::kExtension,
                                              0, "one hundred");

  EXPECT_FALSE(u.IconKey().is_null());
  EXPECT_EQ(apps::mojom::IconType::kExtension, u.IconKey()->icon_type);
  EXPECT_TRUE(u.IconKeyChanged());

  apps::AppUpdate::Merge(state.get(), delta);

  EXPECT_FALSE(u.IconKey().is_null());
  EXPECT_EQ(apps::mojom::IconType::kExtension, u.IconKey()->icon_type);
  EXPECT_FALSE(u.IconKeyChanged());
}
