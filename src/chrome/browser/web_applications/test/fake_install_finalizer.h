// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_INSTALL_FINALIZER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_INSTALL_FINALIZER_H_

#include <map>
#include <memory>
#include <set>

#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

struct WebApplicationInfo;

namespace web_app {

class FakeInstallFinalizer final : public WebAppInstallFinalizer {
 public:
  // Returns what would be the AppId if an app is installed with |url|.
  static AppId GetAppIdForUrl(const GURL& url);

  FakeInstallFinalizer();
  FakeInstallFinalizer(const FakeInstallFinalizer&) = delete;
  FakeInstallFinalizer& operator=(const FakeInstallFinalizer&) = delete;
  ~FakeInstallFinalizer() override;

  // WebAppInstallFinalizer:
  void FinalizeInstall(const WebApplicationInfo& web_app_info,
                       const FinalizeOptions& options,
                       InstallFinalizedCallback callback) override;
  void FinalizeUpdate(const WebApplicationInfo& web_app_info,
                      InstallFinalizedCallback callback) override;
  void UninstallExternalWebApp(
      const AppId& app_id,
      webapps::WebappUninstallSource external_install_source,
      UninstallWebAppCallback callback) override;
  void UninstallExternalWebAppByUrl(
      const GURL& app_url,
      webapps::WebappUninstallSource external_install_source,
      UninstallWebAppCallback callback) override;
  void UninstallWithoutRegistryUpdateFromSync(
      const std::vector<AppId>& web_apps,
      RepeatingUninstallCallback callback) override;
  bool CanUserUninstallWebApp(const AppId& app_id) const override;
  void UninstallWebApp(const AppId& app_id,
                       webapps::WebappUninstallSource uninstall_source,
                       UninstallWebAppCallback callback) override;
  void RetryIncompleteUninstalls(
      const std::vector<AppId>& apps_to_uninstall) override;
  bool WasPreinstalledWebAppUninstalled(const AppId& app_id) const override;
  bool CanReparentTab(const AppId& app_id,
                      bool shortcut_created) const override;
  void ReparentTab(const AppId& app_id,
                   bool shortcut_created,
                   content::WebContents* web_contents) override;
  void SetRemoveSourceCallbackForTesting(
      base::RepeatingCallback<void(const AppId&)>) override;

  void SetNextFinalizeInstallResult(const AppId& app_id,
                                    InstallResultCode code);
  void SetNextUninstallExternalWebAppResult(const GURL& app_url,
                                            bool uninstalled);

  // Uninstall the app and add |app_id| to the map of external extensions
  // uninstalled by the user. May be called on an app that isn't installed to
  // simulate that the app was uninstalled previously.
  void SimulateExternalAppUninstalledByUser(const AppId& app_id);

  std::unique_ptr<WebApplicationInfo> web_app_info() {
    return std::move(web_app_info_copy_);
  }

  const std::vector<FinalizeOptions>& finalize_options_list() const {
    return finalize_options_list_;
  }

  const std::vector<GURL>& uninstall_external_web_app_urls() const {
    return uninstall_external_web_app_urls_;
  }

  int num_reparent_tab_calls() { return num_reparent_tab_calls_; }

 private:
  void Finalize(const WebApplicationInfo& web_app_info,
                InstallResultCode code,
                InstallFinalizedCallback callback);

  std::unique_ptr<WebApplicationInfo> web_app_info_copy_;
  std::vector<FinalizeOptions> finalize_options_list_;
  std::vector<GURL> uninstall_external_web_app_urls_;

  absl::optional<AppId> next_app_id_;
  absl::optional<InstallResultCode> next_result_code_;
  std::map<GURL, bool> next_uninstall_external_web_app_results_;
  std::set<AppId> user_uninstalled_external_apps_;

  int num_reparent_tab_calls_ = 0;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_INSTALL_FINALIZER_H_
