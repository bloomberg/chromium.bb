// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/app_service/public/cpp/app_update.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const apps::mojom::AppType app_type = apps::mojom::AppType::kArc;
const char app_id[] = "abcdefgh";
const char test_name_0[] = "Inigo Montoya";
const char test_name_1[] = "Dread Pirate Roberts";
}  // namespace

class AppUpdateTest : public testing::Test {
 protected:
  apps::mojom::Readiness expect_readiness_;
  bool expect_readiness_changed_;

  std::string expect_name_;
  bool expect_name_changed_;

  std::string expect_short_name_;
  bool expect_short_name_changed_;

  std::string expect_description_;
  bool expect_description_changed_;

  std::vector<std::string> expect_additional_search_terms_;
  bool expect_additional_search_terms_changed_;

  apps::mojom::IconKeyPtr expect_icon_key_;
  bool expect_icon_key_changed_;

  base::Time expect_last_launch_time_;
  bool expect_last_launch_time_changed_;

  base::Time expect_install_time_;
  bool expect_install_time_changed_;

  std::vector<apps::mojom::PermissionPtr> expect_permissions_;
  bool expect_permissions_changed_;

  apps::mojom::InstallSource expect_install_source_;
  bool expect_install_source_changed_;

  apps::mojom::OptionalBool expect_is_platform_app_;
  bool expect_is_platform_app_changed_;

  apps::mojom::OptionalBool expect_recommendable_;
  bool expect_recommendable_changed_;

  apps::mojom::OptionalBool expect_searchable_;
  bool expect_searchable_changed_;

  apps::mojom::OptionalBool expect_show_in_launcher_;
  bool expect_show_in_launcher_changed_;

  apps::mojom::OptionalBool expect_show_in_search_;
  bool expect_show_in_search_changed_;

  apps::mojom::OptionalBool expect_show_in_management_;
  bool expect_show_in_management_changed_;

  static constexpr uint32_t kPermissionTypeLocation = 100;
  static constexpr uint32_t kPermissionTypeNotification = 200;

  apps::mojom::PermissionPtr MakePermission(uint32_t permission_id,
                                            apps::mojom::TriState value) {
    apps::mojom::PermissionPtr permission = apps::mojom::Permission::New();
    permission->permission_id = permission_id;
    permission->value_type = apps::mojom::PermissionValueType::kTriState;
    permission->value = static_cast<uint32_t>(value);
    return permission;
  }

  void ExpectNoChange() {
    expect_readiness_changed_ = false;
    expect_name_changed_ = false;
    expect_short_name_changed_ = false;
    expect_description_changed_ = false;
    expect_additional_search_terms_changed_ = false;
    expect_icon_key_changed_ = false;
    expect_last_launch_time_changed_ = false;
    expect_install_time_changed_ = false;
    expect_permissions_changed_ = false;
    expect_install_source_changed_ = false;
    expect_is_platform_app_changed_ = false;
    expect_recommendable_changed_ = false;
    expect_searchable_changed_ = false;
    expect_show_in_launcher_changed_ = false;
    expect_show_in_search_changed_ = false;
    expect_show_in_management_changed_ = false;
  }

  void CheckExpects(const apps::AppUpdate& u) {
    EXPECT_EQ(expect_readiness_, u.Readiness());
    EXPECT_EQ(expect_readiness_changed_, u.ReadinessChanged());

    EXPECT_EQ(expect_name_, u.Name());
    EXPECT_EQ(expect_name_changed_, u.NameChanged());

    EXPECT_EQ(expect_short_name_, u.ShortName());
    EXPECT_EQ(expect_short_name_changed_, u.ShortNameChanged());

    EXPECT_EQ(expect_description_, u.Description());
    EXPECT_EQ(expect_description_changed_, u.DescriptionChanged());

    EXPECT_EQ(expect_additional_search_terms_, u.AdditionalSearchTerms());
    EXPECT_EQ(expect_additional_search_terms_changed_,
              u.AdditionalSearchTermsChanged());

    EXPECT_EQ(expect_icon_key_, u.IconKey());
    EXPECT_EQ(expect_icon_key_changed_, u.IconKeyChanged());

    EXPECT_EQ(expect_last_launch_time_, u.LastLaunchTime());
    EXPECT_EQ(expect_last_launch_time_changed_, u.LastLaunchTimeChanged());

    EXPECT_EQ(expect_install_time_, u.InstallTime());
    EXPECT_EQ(expect_install_time_changed_, u.InstallTimeChanged());

    EXPECT_EQ(expect_permissions_, u.Permissions());
    EXPECT_EQ(expect_permissions_changed_, u.PermissionsChanged());

    EXPECT_EQ(expect_install_source_, u.InstallSource());
    EXPECT_EQ(expect_install_source_changed_, u.InstallSourceChanged());

    EXPECT_EQ(expect_is_platform_app_, u.IsPlatformApp());
    EXPECT_EQ(expect_is_platform_app_changed_, u.IsPlatformAppChanged());

    EXPECT_EQ(expect_recommendable_, u.Recommendable());
    EXPECT_EQ(expect_recommendable_changed_, u.RecommendableChanged());

    EXPECT_EQ(expect_searchable_, u.Searchable());
    EXPECT_EQ(expect_searchable_changed_, u.SearchableChanged());

    EXPECT_EQ(expect_show_in_launcher_, u.ShowInLauncher());
    EXPECT_EQ(expect_show_in_launcher_changed_, u.ShowInLauncherChanged());

    EXPECT_EQ(expect_show_in_search_, u.ShowInSearch());
    EXPECT_EQ(expect_show_in_search_changed_, u.ShowInSearchChanged());

    EXPECT_EQ(expect_show_in_management_, u.ShowInManagement());
    EXPECT_EQ(expect_show_in_management_changed_, u.ShowInManagementChanged());
  }

  void TestAppUpdate(apps::mojom::App* state, apps::mojom::App* delta) {
    apps::AppUpdate u(state, delta);

    EXPECT_EQ(app_type, u.AppType());
    EXPECT_EQ(app_id, u.AppId());
    EXPECT_EQ(state == nullptr, u.StateIsNull());

    expect_readiness_ = apps::mojom::Readiness::kUnknown;
    expect_name_ = "";
    expect_short_name_ = "";
    expect_description_ = "";
    expect_additional_search_terms_.clear();
    expect_icon_key_ = nullptr;
    expect_last_launch_time_ = base::Time();
    expect_install_time_ = base::Time();
    expect_permissions_.clear();
    expect_install_source_ = apps::mojom::InstallSource::kUnknown;
    expect_is_platform_app_ = apps::mojom::OptionalBool::kUnknown;
    expect_recommendable_ = apps::mojom::OptionalBool::kUnknown;
    expect_searchable_ = apps::mojom::OptionalBool::kUnknown;
    expect_show_in_launcher_ = apps::mojom::OptionalBool::kUnknown;
    expect_show_in_search_ = apps::mojom::OptionalBool::kUnknown;
    expect_show_in_management_ = apps::mojom::OptionalBool::kUnknown;
    ExpectNoChange();
    CheckExpects(u);

    if (delta) {
      delta->name = test_name_0;
      expect_name_ = test_name_0;
      expect_name_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      state->name = test_name_0;
      expect_name_ = test_name_0;
      expect_name_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->readiness = apps::mojom::Readiness::kReady;
      expect_readiness_ = apps::mojom::Readiness::kReady;
      expect_readiness_changed_ = true;
      CheckExpects(u);

      delta->name = base::nullopt;
      expect_name_ = state ? test_name_0 : "";
      expect_name_changed_ = false;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    if (delta) {
      delta->readiness = apps::mojom::Readiness::kDisabledByPolicy;
      expect_readiness_ = apps::mojom::Readiness::kDisabledByPolicy;
      expect_readiness_changed_ = true;
      delta->name = test_name_1;
      expect_name_ = test_name_1;
      expect_name_changed_ = true;
      CheckExpects(u);
    }

    // ShortName tests.

    if (state) {
      state->short_name = "Kate";
      expect_short_name_ = "Kate";
      expect_short_name_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->short_name = "Bob";
      expect_short_name_ = "Bob";
      expect_short_name_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // Description tests.

    if (state) {
      state->description = "Has a cat.";
      expect_description_ = "Has a cat.";
      expect_description_changed_ = true;
    }

    if (delta) {
      delta->description = "Has a dog.";
      expect_description_ = "Has a dog.";
      expect_description_changed_ = true;
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // AdditionalSearchTerms tests.

    if (state) {
      state->additional_search_terms.push_back("cat");
      state->additional_search_terms.push_back("dog");
      expect_additional_search_terms_.push_back("cat");
      expect_additional_search_terms_.push_back("dog");
      expect_additional_search_terms_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      expect_additional_search_terms_.clear();
      delta->additional_search_terms.push_back("horse");
      delta->additional_search_terms.push_back("mouse");
      expect_additional_search_terms_.push_back("horse");
      expect_additional_search_terms_.push_back("mouse");
      expect_additional_search_terms_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // IconKey tests.

    if (state) {
      auto x = apps::mojom::IconKey::New(100, 0, 0);
      state->icon_key = x.Clone();
      expect_icon_key_ = x.Clone();
      expect_icon_key_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      auto x = apps::mojom::IconKey::New(200, 0, 0);
      delta->icon_key = x.Clone();
      expect_icon_key_ = x.Clone();
      expect_icon_key_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // LastLaunchTime tests.

    if (state) {
      state->last_launch_time = base::Time::FromDoubleT(1000.0);
      expect_last_launch_time_ = base::Time::FromDoubleT(1000.0);
      expect_last_launch_time_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->last_launch_time = base::Time::FromDoubleT(1001.0);
      expect_last_launch_time_ = base::Time::FromDoubleT(1001.0);
      expect_last_launch_time_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // InstallTime tests.

    if (state) {
      state->install_time = base::Time::FromDoubleT(2000.0);
      expect_install_time_ = base::Time::FromDoubleT(2000.0);
      expect_install_time_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->install_time = base::Time::FromDoubleT(2001.0);
      expect_install_time_ = base::Time::FromDoubleT(2001.0);
      expect_install_time_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // InstallSource tests.
    if (state) {
      state->install_source = apps::mojom::InstallSource::kUser;
      expect_install_source_ = apps::mojom::InstallSource::kUser;
      expect_install_source_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->install_source = apps::mojom::InstallSource::kPolicy;
      expect_install_source_ = apps::mojom::InstallSource::kPolicy;
      expect_install_source_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // IsPlatformApp tests.

    if (state) {
      state->is_platform_app = apps::mojom::OptionalBool::kFalse;
      expect_is_platform_app_ = apps::mojom::OptionalBool::kFalse;
      expect_is_platform_app_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->is_platform_app = apps::mojom::OptionalBool::kTrue;
      expect_is_platform_app_ = apps::mojom::OptionalBool::kTrue;
      expect_is_platform_app_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // Recommendable tests.

    if (state) {
      state->recommendable = apps::mojom::OptionalBool::kFalse;
      expect_recommendable_ = apps::mojom::OptionalBool::kFalse;
      expect_recommendable_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->recommendable = apps::mojom::OptionalBool::kTrue;
      expect_recommendable_ = apps::mojom::OptionalBool::kTrue;
      expect_recommendable_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // Searchable tests.

    if (state) {
      state->searchable = apps::mojom::OptionalBool::kFalse;
      expect_searchable_ = apps::mojom::OptionalBool::kFalse;
      expect_searchable_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->searchable = apps::mojom::OptionalBool::kTrue;
      expect_searchable_ = apps::mojom::OptionalBool::kTrue;
      expect_searchable_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // ShowInLauncher tests.

    if (state) {
      state->show_in_launcher = apps::mojom::OptionalBool::kFalse;
      expect_show_in_launcher_ = apps::mojom::OptionalBool::kFalse;
      expect_show_in_launcher_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->show_in_launcher = apps::mojom::OptionalBool::kTrue;
      expect_show_in_launcher_ = apps::mojom::OptionalBool::kTrue;
      expect_show_in_launcher_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // ShowInSearch tests.

    if (state) {
      state->show_in_search = apps::mojom::OptionalBool::kFalse;
      expect_show_in_search_ = apps::mojom::OptionalBool::kFalse;
      expect_show_in_search_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->show_in_search = apps::mojom::OptionalBool::kTrue;
      expect_show_in_search_ = apps::mojom::OptionalBool::kTrue;
      expect_show_in_search_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // ShowInManagement tests.

    if (state) {
      state->show_in_management = apps::mojom::OptionalBool::kFalse;
      expect_show_in_management_ = apps::mojom::OptionalBool::kFalse;
      expect_show_in_management_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->show_in_management = apps::mojom::OptionalBool::kTrue;
      expect_show_in_management_ = apps::mojom::OptionalBool::kTrue;
      expect_show_in_management_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // Permission tests.

    if (state) {
      auto p0 = MakePermission(kPermissionTypeLocation,
                               apps::mojom::TriState::kAllow);
      auto p1 = MakePermission(kPermissionTypeNotification,
                               apps::mojom::TriState::kAllow);
      state->permissions.push_back(p0.Clone());
      state->permissions.push_back(p1.Clone());
      expect_permissions_.push_back(p0.Clone());
      expect_permissions_.push_back(p1.Clone());
      expect_permissions_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      expect_permissions_.clear();
      auto p0 = MakePermission(kPermissionTypeNotification,
                               apps::mojom::TriState::kAllow);
      auto p1 = MakePermission(kPermissionTypeLocation,
                               apps::mojom::TriState::kBlock);

      delta->permissions.push_back(p0.Clone());
      delta->permissions.push_back(p1.Clone());
      expect_permissions_.push_back(p0.Clone());
      expect_permissions_.push_back(p1.Clone());
      expect_permissions_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }
  }
};

TEST_F(AppUpdateTest, StateIsNonNull) {
  apps::mojom::AppPtr state = apps::mojom::App::New();
  state->app_type = app_type;
  state->app_id = app_id;

  TestAppUpdate(state.get(), nullptr);
}

TEST_F(AppUpdateTest, DeltaIsNonNull) {
  apps::mojom::AppPtr delta = apps::mojom::App::New();
  delta->app_type = app_type;
  delta->app_id = app_id;

  TestAppUpdate(nullptr, delta.get());
}

TEST_F(AppUpdateTest, BothAreNonNull) {
  apps::mojom::AppPtr state = apps::mojom::App::New();
  state->app_type = app_type;
  state->app_id = app_id;

  apps::mojom::AppPtr delta = apps::mojom::App::New();
  delta->app_type = app_type;
  delta->app_id = app_id;

  TestAppUpdate(state.get(), delta.get());
}
