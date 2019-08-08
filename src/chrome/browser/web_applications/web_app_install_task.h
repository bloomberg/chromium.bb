// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_TASK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_TASK_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "content/public/browser/web_contents_observer.h"

class Profile;
struct WebApplicationInfo;

namespace blink {
struct Manifest;
}

namespace content {
class WebContents;
}

namespace web_app {

class InstallFinalizer;
class WebAppDataRetriever;

class WebAppInstallTask : content::WebContentsObserver {
 public:
  WebAppInstallTask(Profile* profile, InstallFinalizer* install_finalizer);
  ~WebAppInstallTask() override;

  // Checks a WebApp installability, retrieves manifest and icons and
  // than performs the actual installation.
  void InstallWebAppFromManifest(
      content::WebContents* web_contents,
      WebappInstallSource install_source,
      InstallManager::WebAppInstallDialogCallback dialog_callback,
      InstallManager::OnceInstallCallback callback);

  // This method infers WebApp info from the blink renderer process
  // and than retrieves a manifest in a way similar to
  // |InstallWebAppFromManifest|. If manifest is incomplete or missing, the
  // inferred info is used.
  void InstallWebAppFromManifestWithFallback(
      content::WebContents* web_contents,
      bool force_shortcut_app,
      WebappInstallSource install_source,
      InstallManager::WebAppInstallDialogCallback dialog_callback,
      InstallManager::OnceInstallCallback callback);

  // WebContentsObserver:
  void WebContentsDestroyed() override;

  void SetDataRetrieverForTesting(
      std::unique_ptr<WebAppDataRetriever> data_retriever);
  void SetInstallFinalizerForTesting(InstallFinalizer* install_finalizer);

 private:
  void CheckInstallPreconditions();

  // Calling the callback may destroy |this| task. Callers shoudln't work with
  // any |this| class members after calling it.
  void CallInstallCallback(const AppId& app_id, InstallResultCode code);

  // Checks if any errors occurred while |this| was async awaiting. All On*
  // completion handlers below must return early if this is true. Also, if
  // ShouldStopInstall is true, install_callback_ is already invoked or may be
  // invoked later: All On* completion handlers don't need to call
  // install_callback_.
  bool ShouldStopInstall() const;

  void OnGetWebApplicationInfo(
      bool force_shortcut_app,
      std::unique_ptr<WebApplicationInfo> web_app_info);
  void OnDidPerformInstallableCheck(
      std::unique_ptr<WebApplicationInfo> web_app_info,
      bool force_shortcut_app,
      const blink::Manifest& manifest,
      bool is_installable);
  void OnIconsRetrieved(std::unique_ptr<WebApplicationInfo> web_app_info,
                        ForInstallableSite for_installable_site,
                        IconsMap icons_map);
  void OnDialogCompleted(ForInstallableSite for_installable_site,
                         bool user_accepted,
                         std::unique_ptr<WebApplicationInfo> web_app_info);
  void OnInstallFinalized(std::unique_ptr<WebApplicationInfo> web_app_info,
                          const AppId& app_id,
                          InstallResultCode code);
  void OnShortcutsCreated(std::unique_ptr<WebApplicationInfo> web_app_info,
                          const AppId& app_id,
                          bool shortcut_created);

  InstallManager::WebAppInstallDialogCallback dialog_callback_;
  InstallManager::OnceInstallCallback install_callback_;

  // The mechanism via which the app creation was triggered.
  static constexpr WebappInstallSource kNoInstallSource =
      WebappInstallSource::COUNT;
  WebappInstallSource install_source_ = kNoInstallSource;

  std::unique_ptr<WebAppDataRetriever> data_retriever_;

  InstallFinalizer* install_finalizer_;
  Profile* profile_;

  base::WeakPtrFactory<WebAppInstallTask> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppInstallTask);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_TASK_H_
