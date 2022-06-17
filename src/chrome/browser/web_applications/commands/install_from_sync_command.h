// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_FROM_SYNC_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_FROM_SYNC_COMMAND_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_logging.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_url_loader.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

class Profile;

namespace web_app {

class InstallFromSyncCommand : public WebAppCommand {
 public:
  struct Params {
    Params() = delete;
    ~Params();
    Params(const Params&);
    Params(AppId app_id,
           const absl::optional<std::string>& manifest_id,
           const GURL& start_url,
           const std::string& title,
           const GURL& scope,
           const absl::optional<SkColor>& theme_color,
           const absl::optional<UserDisplayMode>& user_display_mode,
           const std::vector<apps::IconInfo>& icons);
    const AppId app_id;
    const absl::optional<std::string> manifest_id;
    const GURL start_url;
    const std::string title;
    const GURL scope;
    const absl::optional<SkColor> theme_color;
    const absl::optional<UserDisplayMode> user_display_mode;
    const std::vector<apps::IconInfo> icons;
  };
  using DataRetrieverFactory =
      base::RepeatingCallback<std::unique_ptr<WebAppDataRetriever>()>;

  InstallFromSyncCommand(
      WebAppUrlLoader* url_loader,
      Profile* profile,
      WebAppInstallFinalizer* finalizer,
      WebAppRegistrar* registrar,
      std::unique_ptr<WebAppDataRetriever> web_app_data_retriever,
      const Params& params,
      OnceInstallCallback install_callback);
  ~InstallFromSyncCommand() override;

  base::Value ToDebugValue() const override;

  void OnSyncSourceRemoved() override;

  void OnShutdown() override;

  void Start() override;

  void SetFallbackTriggeredForTesting(
      base::OnceCallback<void(webapps::InstallResultCode code)> callback);

 private:
  void OnWebAppUrlLoadedGetWebAppInstallInfo(WebAppUrlLoader::Result result);

  void OnGetWebAppInstallInfo(std::unique_ptr<WebAppInstallInfo> web_app_info);

  void OnDidPerformInstallableCheck(blink::mojom::ManifestPtr opt_manifest,
                                    const GURL& manifest_url,
                                    bool valid_manifest_for_web_app,
                                    bool is_installable);

  enum class FinalizeMode { kNormalWebAppInfo, kFallbackWebAppInfo };

  void OnIconsRetrievedFinalizeInstall(
      FinalizeMode mode,
      IconsDownloadedResult result,
      IconsMap icons_map,
      DownloadedIconsHttpResults icons_http_results);

  void OnInstallFinalized(FinalizeMode mode,
                          const AppId& app_id,
                          webapps::InstallResultCode code,
                          OsHooksErrors os_hooks_errors);

  void InstallFallback(webapps::InstallResultCode error_code);

  void ReportResultAndDestroy(const AppId& app_id,
                              webapps::InstallResultCode code);

  const base::raw_ptr<WebAppUrlLoader> url_loader_;
  const base::raw_ptr<Profile> profile_;
  const base::raw_ptr<WebAppInstallFinalizer> finalizer_;
  const base::raw_ptr<WebAppRegistrar> registrar_;
  const std::unique_ptr<WebAppDataRetriever> data_retriever_;
  const Params params_;
  OnceInstallCallback install_callback_;

  std::unique_ptr<WebAppInstallInfo> install_info_;
  std::unique_ptr<WebAppInstallInfo> fallback_install_info_;

  base::Value::List error_log_;
  InstallErrorLogEntry install_error_log_entry_;

  base::OnceCallback<void(webapps::InstallResultCode code)>
      fallback_triggered_for_testing_;

  base::WeakPtrFactory<InstallFromSyncCommand> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_FROM_SYNC_COMMAND_H_
