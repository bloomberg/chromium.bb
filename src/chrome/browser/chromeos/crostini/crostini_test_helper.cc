// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_test_helper.h"

#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/scoped_user_manager.h"

using vm_tools::apps::App;
using vm_tools::apps::ApplicationList;

namespace crostini {

CrostiniTestHelper::CrostiniTestHelper(Profile* profile, bool enable_crostini)
    : profile_(profile) {
  scoped_feature_list_.InitAndEnableFeature(features::kCrostini);

  chromeos::ProfileHelper::SetAlwaysReturnPrimaryUserForTesting(true);
  chromeos::MockUserManager* mock_user_manager =
      new testing::NiceMock<chromeos::MockUserManager>();
  mock_user_manager->SetActiveUser(
      AccountId::FromUserEmail("test@example.com"));
  scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
      base::WrapUnique(mock_user_manager));

  if (enable_crostini)
    EnableCrostini(profile);

  current_apps_.set_vm_name(kCrostiniDefaultVmName);
  current_apps_.set_container_name(kCrostiniDefaultContainerName);
}

CrostiniTestHelper::~CrostiniTestHelper() {
  chromeos::ProfileHelper::SetAlwaysReturnPrimaryUserForTesting(false);
  DisableCrostini(profile_);
}

void CrostiniTestHelper::SetupDummyApps() {
  // This updates the registry for us.
  AddApp(BasicApp("dummy1"));
  AddApp(BasicApp("dummy2"));
}

App CrostiniTestHelper::GetApp(int i) {
  return current_apps_.apps(i);
}

void CrostiniTestHelper::AddApp(const vm_tools::apps::App& app) {
  for (int i = 0; i < current_apps_.apps_size(); ++i) {
    if (current_apps_.apps(i).desktop_file_id() == app.desktop_file_id()) {
      *current_apps_.mutable_apps(i) = app;
      UpdateRegistry();
      return;
    }
  }
  *current_apps_.add_apps() = app;
  UpdateRegistry();
}

void CrostiniTestHelper::RemoveApp(int i) {
  auto* apps = current_apps_.mutable_apps();
  apps->erase(apps->begin() + i);
  UpdateRegistry();
}

void CrostiniTestHelper::UpdateAppKeywords(
    vm_tools::apps::App& app,
    const std::map<std::string, std::set<std::string>>& keywords) {
  for (const auto& keywords_entry : keywords) {
    auto* strings_with_locale = app.mutable_keywords()->add_values();
    strings_with_locale->set_locale(keywords_entry.first);
    for (const auto& curr_keyword : keywords_entry.second) {
      strings_with_locale->add_value(curr_keyword);
    }
  }
}

// static
void CrostiniTestHelper::EnableCrostini(Profile* profile) {
  profile->GetPrefs()->SetBoolean(crostini::prefs::kCrostiniEnabled, true);
}
// static
void CrostiniTestHelper::DisableCrostini(Profile* profile) {
  profile->GetPrefs()->SetBoolean(crostini::prefs::kCrostiniEnabled, false);
}

// static
std::string CrostiniTestHelper::GenerateAppId(
    const std::string& desktop_file_id,
    const std::string& vm_name,
    const std::string& container_name) {
  return crx_file::id_util::GenerateId("crostini:" + vm_name + "/" +
                                       container_name + "/" + desktop_file_id);
}

// static
App CrostiniTestHelper::BasicApp(const std::string& desktop_file_id,
                                 const std::string& name,
                                 bool no_display) {
  App app;
  app.set_desktop_file_id(desktop_file_id);
  App::LocaleString::Entry* entry = app.mutable_name()->add_values();
  entry->set_locale(std::string());
  entry->set_value(name.empty() ? desktop_file_id : name);
  app.set_no_display(no_display);
  return app;
}

// static
ApplicationList CrostiniTestHelper::BasicAppList(
    const std::string& desktop_file_id,
    const std::string& vm_name,
    const std::string& container_name) {
  ApplicationList app_list;
  app_list.set_vm_name(vm_name);
  app_list.set_container_name(container_name);
  *app_list.add_apps() = BasicApp(desktop_file_id);
  return app_list;
}

void CrostiniTestHelper::UpdateRegistry() {
  crostini::CrostiniRegistryServiceFactory::GetForProfile(profile_)
      ->UpdateApplicationList(current_apps_);
}

}  // namespace crostini
