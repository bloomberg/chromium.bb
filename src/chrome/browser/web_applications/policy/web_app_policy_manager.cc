// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/policy/web_app_policy_constants.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace web_app {

WebAppPolicyManager::WebAppPolicyManager(Profile* profile,
                                         PendingAppManager* pending_app_manager)
    : profile_(profile),
      pref_service_(profile_->GetPrefs()),
      pending_app_manager_(pending_app_manager) {}

WebAppPolicyManager::~WebAppPolicyManager() = default;

void WebAppPolicyManager::Start() {
  content::BrowserThread::PostAfterStartupTask(
      FROM_HERE,
      base::CreateSingleThreadTaskRunnerWithTraits(
          {content::BrowserThread::UI}),
      base::BindOnce(&WebAppPolicyManager::
                         InitChangeRegistrarAndRefreshPolicyInstalledApps,
                     weak_ptr_factory_.GetWeakPtr()));
}

// static
void WebAppPolicyManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kWebAppInstallForceList);
}

void WebAppPolicyManager::InitChangeRegistrarAndRefreshPolicyInstalledApps() {
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kWebAppInstallForceList,
      base::BindRepeating(&WebAppPolicyManager::RefreshPolicyInstalledApps,
                          weak_ptr_factory_.GetWeakPtr()));

  RefreshPolicyInstalledApps();
}

void WebAppPolicyManager::RefreshPolicyInstalledApps() {
  const base::Value* web_apps =
      pref_service_->GetList(prefs::kWebAppInstallForceList);
  std::vector<PendingAppManager::AppInfo> apps_to_install;
  for (const base::Value& info : web_apps->GetList()) {
    const base::Value& url = *info.FindKey(kUrlKey);
    const base::Value* launch_container = info.FindKey(kLaunchContainerKey);

    DCHECK(!launch_container ||
           launch_container->GetString() == kLaunchContainerWindowValue ||
           launch_container->GetString() == kLaunchContainerTabValue);

    LaunchContainer container;
    if (!launch_container)
      container = LaunchContainer::kDefault;
    else if (launch_container->GetString() == kLaunchContainerWindowValue)
      container = LaunchContainer::kWindow;
    else
      container = LaunchContainer::kTab;

    web_app::PendingAppManager::AppInfo app_info(
        GURL(std::move(url.GetString())), container,
        web_app::InstallSource::kExternalPolicy);
    app_info.create_shortcuts = false;

    // There is a separate policy to create shortcuts/pin apps to shelf.
    apps_to_install.push_back(std::move(app_info));
  }

  pending_app_manager_->SynchronizeInstalledApps(
      std::move(apps_to_install), InstallSource::kExternalPolicy);
}

}  // namespace web_app
