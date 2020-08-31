// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/web_applications/web_app_install_manager.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_data_retriever.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_install_task.h"
#include "chrome/common/web_application_info.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

namespace {

#if defined(OS_CHROMEOS)
constexpr bool kLocallyInstallWebAppsOnSync = true;
#else
constexpr bool kLocallyInstallWebAppsOnSync = false;
#endif

InstallManager::InstallParams CreateSyncInstallParams(
    const GURL& start_url,
    const base::string16& app_name,
    DisplayMode user_display_mode) {
  InstallManager::InstallParams params;
  params.user_display_mode = user_display_mode;
  params.fallback_start_url = start_url;
  params.fallback_app_name = app_name;
  // If app is not locally installed then no OS integration like OS shortcuts.
  params.locally_installed = kLocallyInstallWebAppsOnSync;
  params.add_to_applications_menu = kLocallyInstallWebAppsOnSync;
  params.add_to_desktop = kLocallyInstallWebAppsOnSync;
  // Never add the app to the quick launch bar after sync.
  params.add_to_quick_launch_bar = false;
  return params;
}

}  // namespace

WebAppInstallManager::WebAppInstallManager(Profile* profile)
    : InstallManager(profile),
      url_loader_(std::make_unique<WebAppUrlLoader>()) {
  data_retriever_factory_ = base::BindRepeating(
      []() { return std::make_unique<WebAppDataRetriever>(); });
}

WebAppInstallManager::~WebAppInstallManager() = default;

void WebAppInstallManager::LoadWebAppAndCheckInstallability(
    const GURL& web_app_url,
    WebappInstallSource install_source,
    WebAppInstallabilityCheckCallback callback) {
  auto task = std::make_unique<WebAppInstallTask>(
      profile(), registrar(), shortcut_manager(), file_handler_manager(),
      finalizer(), data_retriever_factory_.Run());

  task->LoadWebAppAndCheckInstallability(
      web_app_url, install_source, url_loader_.get(),
      base::BindOnce(
          &WebAppInstallManager::OnLoadWebAppAndCheckInstallabilityCompleted,
          base::Unretained(this), task.get(), std::move(callback)));

  tasks_.insert(std::move(task));
}

void WebAppInstallManager::InstallWebAppFromManifest(
    content::WebContents* contents,
    WebappInstallSource install_source,
    WebAppInstallDialogCallback dialog_callback,
    OnceInstallCallback callback) {
  auto task = std::make_unique<WebAppInstallTask>(
      profile(), registrar(), shortcut_manager(), file_handler_manager(),
      finalizer(), data_retriever_factory_.Run());
  task->InstallWebAppFromManifest(
      contents, install_source, std::move(dialog_callback),
      base::BindOnce(&WebAppInstallManager::OnInstallTaskCompleted,
                     base::Unretained(this), task.get(), std::move(callback)));

  tasks_.insert(std::move(task));
}

void WebAppInstallManager::InstallWebAppFromManifestWithFallback(
    content::WebContents* contents,
    bool force_shortcut_app,
    WebappInstallSource install_source,
    WebAppInstallDialogCallback dialog_callback,
    OnceInstallCallback callback) {
  auto task = std::make_unique<WebAppInstallTask>(
      profile(), registrar(), shortcut_manager(), file_handler_manager(),
      finalizer(), data_retriever_factory_.Run());
  task->InstallWebAppFromManifestWithFallback(
      contents, force_shortcut_app, install_source, std::move(dialog_callback),
      base::BindOnce(&WebAppInstallManager::OnInstallTaskCompleted,
                     base::Unretained(this), task.get(), std::move(callback)));

  tasks_.insert(std::move(task));
}

void WebAppInstallManager::InstallWebAppFromInfo(
    std::unique_ptr<WebApplicationInfo> web_application_info,
    ForInstallableSite for_installable_site,
    WebappInstallSource install_source,
    OnceInstallCallback callback) {
  auto task = std::make_unique<WebAppInstallTask>(
      profile(), registrar(), shortcut_manager(), file_handler_manager(),
      finalizer(), data_retriever_factory_.Run());
  task->InstallWebAppFromInfo(
      std::move(web_application_info), for_installable_site, install_source,
      base::BindOnce(&WebAppInstallManager::OnInstallTaskCompleted,
                     base::Unretained(this), task.get(), std::move(callback)));

  tasks_.insert(std::move(task));
}

void WebAppInstallManager::InstallWebAppWithParams(
    content::WebContents* web_contents,
    const InstallParams& install_params,
    WebappInstallSource install_source,
    OnceInstallCallback callback) {
  auto task = std::make_unique<WebAppInstallTask>(
      profile(), registrar(), shortcut_manager(), file_handler_manager(),
      finalizer(), data_retriever_factory_.Run());
  task->InstallWebAppWithParams(
      web_contents, install_params, install_source,
      base::BindOnce(&WebAppInstallManager::OnInstallTaskCompleted,
                     base::Unretained(this), task.get(), std::move(callback)));

  tasks_.insert(std::move(task));
}

void WebAppInstallManager::InstallBookmarkAppFromSync(
    const AppId& bookmark_app_id,
    std::unique_ptr<WebApplicationInfo> web_application_info,
    OnceInstallCallback callback) {
  // Skip sync update if app exists.
  // All manifest fields will be set locally via update (see crbug.com/926083)
  // so we must not sync them in order to avoid a device-to-device sync war.
  if (registrar()->IsInstalled(bookmark_app_id)) {
    std::move(callback).Run(bookmark_app_id,
                            InstallResultCode::kSuccessAlreadyInstalled);
    return;
  }

  // If bookmark_app_id is not installed enqueue full background installation
  // flow. This install may produce a web app or an extension-based bookmark
  // app, depending on the BMO flag.
  GURL launch_url = web_application_info->app_url;

  auto task = std::make_unique<WebAppInstallTask>(
      profile(), registrar(), shortcut_manager(), file_handler_manager(),
      finalizer(), data_retriever_factory_.Run());

  task->ExpectAppId(bookmark_app_id);
  task->SetInstallParams(CreateSyncInstallParams(
      launch_url, web_application_info->title,
      web_application_info->open_as_window ? DisplayMode::kStandalone
                                           : DisplayMode::kBrowser));

  OnceInstallCallback task_completed_callback = base::BindOnce(
      &WebAppInstallManager::
          LoadAndInstallWebAppFromManifestWithFallbackCompleted_ForBookmarkAppSync,
      base::Unretained(this), bookmark_app_id, std::move(web_application_info),
      std::move(callback));

  base::OnceClosure start_task = base::BindOnce(
      &WebAppInstallTask::LoadAndInstallWebAppFromManifestWithFallback,
      base::Unretained(task.get()), launch_url, EnsureWebContentsCreated(),
      base::Unretained(url_loader_.get()), WebappInstallSource::SYNC,
      base::BindOnce(&WebAppInstallManager::OnQueuedTaskCompleted,
                     base::Unretained(this), task.get(),
                     std::move(task_completed_callback)));

  EnqueueTask(std::move(task), std::move(start_task));
}

void WebAppInstallManager::UpdateWebAppFromInfo(
    const AppId& app_id,
    std::unique_ptr<WebApplicationInfo> web_application_info,
    OnceInstallCallback callback) {
  auto task = std::make_unique<WebAppInstallTask>(
      profile(), registrar(), shortcut_manager(), file_handler_manager(),
      finalizer(), data_retriever_factory_.Run());

  base::OnceClosure start_task = base::BindOnce(
      &WebAppInstallTask::UpdateWebAppFromInfo, base::Unretained(task.get()),
      EnsureWebContentsCreated(), app_id, std::move(web_application_info),
      base::BindOnce(&WebAppInstallManager::OnQueuedTaskCompleted,
                     base::Unretained(this), task.get(), std::move(callback)));

  EnqueueTask(std::move(task), std::move(start_task));
}

void WebAppInstallManager::Shutdown() {
  tasks_.clear();
  {
    TaskQueue empty;
    task_queue_.swap(empty);
  }
  web_contents_.reset();
}

void WebAppInstallManager::InstallWebAppsAfterSync(
    std::vector<WebApp*> web_apps,
    RepeatingInstallCallback callback) {
  for (WebApp* web_app : web_apps) {
    DCHECK(web_app->is_in_sync_install());

    auto task = std::make_unique<WebAppInstallTask>(
        profile(), registrar(), shortcut_manager(), file_handler_manager(),
        finalizer(), data_retriever_factory_.Run());

    task->ExpectAppId(web_app->app_id());
    task->SetInstallParams(CreateSyncInstallParams(
        web_app->launch_url(), base::UTF8ToUTF16(web_app->sync_data().name),
        web_app->user_display_mode()));

    OnceInstallCallback sync_install_callback =
        base::BindOnce(&WebAppInstallManager::OnWebAppInstalledAfterSync,
                       base::Unretained(this), web_app->app_id(), callback);

    base::OnceClosure start_task = base::BindOnce(
        &WebAppInstallTask::LoadAndInstallWebAppFromManifestWithFallback,
        base::Unretained(task.get()), web_app->launch_url(),
        EnsureWebContentsCreated(), base::Unretained(url_loader_.get()),
        WebappInstallSource::SYNC,
        base::BindOnce(&WebAppInstallManager::OnQueuedTaskCompleted,
                       base::Unretained(this), task.get(),
                       std::move(sync_install_callback)));

    EnqueueTask(std::move(task), std::move(start_task));
  }
}

void WebAppInstallManager::UninstallWebAppsAfterSync(
    std::vector<std::unique_ptr<WebApp>> web_apps,
    RepeatingUninstallCallback callback) {
  for (std::unique_ptr<WebApp>& web_app : web_apps) {
    const AppId& app_id = web_app->app_id();

    finalizer()->FinalizeUninstallAfterSync(
        app_id,
        base::BindOnce(&WebAppInstallManager::OnWebAppUninstalledAfterSync,
                       weak_ptr_factory_.GetWeakPtr(), std::move(web_app),
                       callback));
  }
}

void WebAppInstallManager::SetUrlLoaderForTesting(
    std::unique_ptr<WebAppUrlLoader> url_loader) {
  url_loader_ = std::move(url_loader);
}

void WebAppInstallManager::
    LoadAndInstallWebAppFromManifestWithFallbackCompleted_ForBookmarkAppSync(
        const AppId& bookmark_app_id,
        std::unique_ptr<WebApplicationInfo> web_application_info,
        OnceInstallCallback callback,
        const AppId& web_app_id,
        InstallResultCode code) {
  // TODO(loyso): Record |code| for this specific case in
  // Webapp.BookmarkAppInstalledAfterSyncResult UMA.
  if (IsSuccess(code)) {
    DCHECK_EQ(bookmark_app_id, web_app_id);
    std::move(callback).Run(web_app_id, code);
    return;
  }

  // Install failed. Do the fallback install from info fetching just icon URLs.
  auto task = std::make_unique<WebAppInstallTask>(
      profile(), registrar(), shortcut_manager(), file_handler_manager(),
      finalizer(), data_retriever_factory_.Run());

  InstallFinalizer::FinalizeOptions finalize_options;
  finalize_options.install_source = WebappInstallSource::SYNC;
  finalize_options.locally_installed = kLocallyInstallWebAppsOnSync;

  base::OnceClosure start_task = base::BindOnce(
      &WebAppInstallTask::InstallWebAppFromInfoRetrieveIcons,
      base::Unretained(task.get()), EnsureWebContentsCreated(),
      std::move(web_application_info), finalize_options,
      base::BindOnce(&WebAppInstallManager::OnQueuedTaskCompleted,
                     base::Unretained(this), task.get(), std::move(callback)));

  EnqueueTask(std::move(task), std::move(start_task));
}

void WebAppInstallManager::EnqueueTask(std::unique_ptr<WebAppInstallTask> task,
                                       base::OnceClosure start_task) {
  DCHECK(web_contents_);

  tasks_.insert(std::move(task));
  task_queue_.push(std::move(start_task));

  if (web_contents_ready_)
    MaybeStartQueuedTask();
}

void WebAppInstallManager::MaybeStartQueuedTask() {
  DCHECK(web_contents_ready_);
  DCHECK(!task_queue_.empty());

  if (is_running_queued_task_)
    return;

  is_running_queued_task_ = true;

  base::OnceClosure task_closure = std::move(task_queue_.front());
  task_queue_.pop();

  std::move(task_closure).Run();
}

void WebAppInstallManager::SetDataRetrieverFactoryForTesting(
    DataRetrieverFactory data_retriever_factory) {
  data_retriever_factory_ = std::move(data_retriever_factory);
}

void WebAppInstallManager::DeleteTask(WebAppInstallTask* task) {
  DCHECK(tasks_.contains(task));
  tasks_.erase(task);
}

void WebAppInstallManager::OnInstallTaskCompleted(WebAppInstallTask* task,
                                                  OnceInstallCallback callback,
                                                  const AppId& app_id,
                                                  InstallResultCode code) {
  DeleteTask(task);

  std::move(callback).Run(app_id, code);
}

void WebAppInstallManager::OnQueuedTaskCompleted(WebAppInstallTask* task,
                                                 OnceInstallCallback callback,
                                                 const AppId& app_id,
                                                 InstallResultCode code) {
  DCHECK(is_running_queued_task_);
  is_running_queued_task_ = false;

  OnInstallTaskCompleted(task, std::move(callback), app_id, code);
  task = nullptr;

  // |callback| may have started another task.
  if (is_running_queued_task_)
    return;

  if (task_queue_.empty()) {
    web_contents_.reset();
    web_contents_ready_ = false;
  } else {
    MaybeStartQueuedTask();
  }
}

void WebAppInstallManager::OnLoadWebAppAndCheckInstallabilityCompleted(
    WebAppInstallTask* task,
    WebAppInstallabilityCheckCallback callback,
    std::unique_ptr<content::WebContents> web_contents,
    const AppId& app_id,
    InstallResultCode code) {
  DeleteTask(task);

  InstallableCheckResult result;
  base::Optional<AppId> opt_app_id;
  if (IsSuccess(code)) {
    if (!app_id.empty() && registrar()->IsInstalled(app_id)) {
      result = InstallableCheckResult::kAlreadyInstalled;
      opt_app_id = app_id;
    } else {
      result = InstallableCheckResult::kInstallable;
    }
  } else {
    result = InstallableCheckResult::kNotInstallable;
  }

  std::move(callback).Run(std::move(web_contents), result, opt_app_id);
}

void WebAppInstallManager::OnWebAppInstalledAfterSync(
    const AppId& app_in_sync_install_id,
    OnceInstallCallback callback,
    const AppId& installed_app_id,
    InstallResultCode code) {
  // TODO(loyso): Record |code| for this specific case in
  // Webapp.SyncInitiatedFullInstallResult UMA.
  if (IsSuccess(code)) {
    DCHECK_EQ(app_in_sync_install_id, installed_app_id);
    std::move(callback).Run(installed_app_id, code);
  } else {
    // Install failed. Do the fallback install to generate icons.
    finalizer()->FinalizeFallbackInstallAfterSync(app_in_sync_install_id,
                                                  std::move(callback));
  }
}

void WebAppInstallManager::OnWebAppUninstalledAfterSync(
    std::unique_ptr<WebApp> web_app,
    OnceUninstallCallback callback,
    bool uninstalled) {
  UMA_HISTOGRAM_BOOLEAN("Webapp.SyncInitiatedUninstallResult", uninstalled);
  std::move(callback).Run(web_app->app_id(), uninstalled);
  // web_app data is destroyed here.
}

content::WebContents* WebAppInstallManager::EnsureWebContentsCreated() {
  if (web_contents_)
    return web_contents_.get();

  DCHECK(!web_contents_ready_);

  web_contents_ = WebAppInstallTask::CreateWebContents(profile());

  // Load about:blank so that the process actually starts.
  url_loader_->LoadUrl(GURL("about:blank"), web_contents_.get(),
                       WebAppUrlLoader::UrlComparison::kExact,
                       base::BindOnce(&WebAppInstallManager::OnWebContentsReady,
                                      weak_ptr_factory_.GetWeakPtr()));

  return web_contents_.get();
}

void WebAppInstallManager::OnWebContentsReady(WebAppUrlLoader::Result result) {
  DCHECK_EQ(WebAppUrlLoader::Result::kUrlLoaded, result);
  web_contents_ready_ = true;

  MaybeStartQueuedTask();
}

}  // namespace web_app
