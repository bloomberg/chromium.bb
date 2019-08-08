// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_TEST_PENDING_APP_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_TEST_PENDING_APP_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "url/gurl.h"

namespace web_app {

class TestPendingAppManager : public PendingAppManager {
 public:
  TestPendingAppManager();
  ~TestPendingAppManager() override;

  // The foo_requests methods may return duplicates, if the underlying
  // InstallApps or UninstallApps arguments do. The deduped_foo_count methods
  // only count new installs or new uninstalls.

  const std::vector<InstallOptions>& install_requests() const {
    return install_requests_;
  }
  const std::vector<GURL>& uninstall_requests() const {
    return uninstall_requests_;
  }

  int deduped_install_count() const { return deduped_install_count_; }
  int deduped_uninstall_count() const { return deduped_uninstall_count_; }

  void ResetCounts() {
    deduped_install_count_ = 0;
    deduped_uninstall_count_ = 0;
  }

  const std::map<GURL, InstallSource>& installed_apps() const {
    return installed_apps_;
  }

  void SimulatePreviouslyInstalledApp(const GURL& url,
                                      InstallSource install_source);

  void SetInstallResultCode(InstallResultCode result_code);

  // PendingAppManager:
  void Install(InstallOptions install_options,
               OnceInstallCallback callback) override;
  void InstallApps(std::vector<InstallOptions> install_options_list,
                   const RepeatingInstallCallback& callback) override;
  void UninstallApps(std::vector<GURL> uninstall_urls,
                     const UninstallCallback& callback) override;
  std::vector<GURL> GetInstalledAppUrls(
      InstallSource install_source) const override;
  base::Optional<AppId> LookupAppId(const GURL& url) const override;
  bool HasAppIdWithInstallSource(
      const AppId& app_id,
      web_app::InstallSource install_source) const override;
  void Shutdown() override {}

 private:
  void DoInstall(InstallOptions install_options, OnceInstallCallback callback);
  std::vector<InstallOptions> install_requests_;
  std::vector<GURL> uninstall_requests_;

  int deduped_install_count_;
  int deduped_uninstall_count_;

  std::map<GURL, InstallSource> installed_apps_;
  InstallResultCode install_result_code_ = InstallResultCode::kSuccess;

  base::WeakPtrFactory<TestPendingAppManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TestPendingAppManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_TEST_PENDING_APP_MANAGER_H_
