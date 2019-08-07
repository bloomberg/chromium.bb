// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/test_pending_app_manager.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/sequenced_task_runner.h"
#include "base/test/bind_test_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "url/gurl.h"

namespace web_app {

TestPendingAppManager::TestPendingAppManager()
    : deduped_install_count_(0), deduped_uninstall_count_(0) {}

TestPendingAppManager::~TestPendingAppManager() = default;

void TestPendingAppManager::SimulatePreviouslyInstalledApp(
    const GURL& url,
    InstallSource install_source) {
  installed_apps_[url] = install_source;
}

void TestPendingAppManager::SetInstallResultCode(
    InstallResultCode result_code) {
  install_result_code_ = result_code;
}

void TestPendingAppManager::Install(InstallOptions install_options,
                                    OnceInstallCallback callback) {
  // TODO(nigeltao): Add error simulation when error codes are added to the API.
  auto do_install = base::BindLambdaForTesting(
      [this, install_options](OnceInstallCallback callback) {
        auto i = installed_apps_.find(install_options.url);
        if (i == installed_apps_.end()) {
          installed_apps_[install_options.url] = install_options.install_source;
          deduped_install_count_++;
        }
        install_requests_.push_back(install_options);

        std::move(callback).Run(install_options.url, install_result_code_);
      });
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(do_install, std::move(callback)));
}

void TestPendingAppManager::InstallApps(
    std::vector<InstallOptions> install_options_list,
    const RepeatingInstallCallback& callback) {
  for (auto& install_options : install_options_list)
    Install(std::move(install_options), callback);
}

void TestPendingAppManager::UninstallApps(std::vector<GURL> uninstall_urls,
                                          const UninstallCallback& callback) {
  auto do_uninstall =
      base::BindLambdaForTesting([&](UninstallCallback callback, GURL url) {
        auto i = installed_apps_.find(url);
        if (i != installed_apps_.end()) {
          installed_apps_.erase(i);
          deduped_uninstall_count_++;
        }

        uninstall_requests_.push_back(url);
        callback.Run(url, true /* succeeded */);
      });
  for (const auto& url : uninstall_urls) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(do_uninstall, callback, url));
  }
}

std::vector<GURL> TestPendingAppManager::GetInstalledAppUrls(
    InstallSource install_source) const {
  std::vector<GURL> urls;
  for (auto& it : installed_apps_) {
    if (it.second == install_source) {
      urls.emplace_back(it.first);
    }
  }
  return urls;
}

base::Optional<AppId> TestPendingAppManager::LookupAppId(
    const GURL& url) const {
  return base::Optional<std::string>();
}

bool TestPendingAppManager::HasAppIdWithInstallSource(
    const AppId& app_id,
    web_app::InstallSource install_source) const {
  return false;
}

}  // namespace web_app
