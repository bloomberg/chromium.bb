// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/externally_managed_app_install_task.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_installation_utils.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/installable/installable_params.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

ExternallyManagedAppInstallTask::ExternallyManagedAppInstallTask(
    Profile* profile,
    WebAppUrlLoader* url_loader,
    WebAppRegistrar* registrar,
    OsIntegrationManager* os_integration_manager,
    WebAppUiManager* ui_manager,
    WebAppInstallFinalizer* install_finalizer,
    WebAppInstallManager* install_manager,
    ExternalInstallOptions install_options)
    : profile_(profile),
      url_loader_(url_loader),
      registrar_(registrar),
      os_integration_manager_(os_integration_manager),
      install_finalizer_(install_finalizer),
      install_manager_(install_manager),
      ui_manager_(ui_manager),
      externally_installed_app_prefs_(profile_->GetPrefs()),
      install_options_(std::move(install_options)) {}

ExternallyManagedAppInstallTask::~ExternallyManagedAppInstallTask() = default;

void ExternallyManagedAppInstallTask::Install(
    content::WebContents* web_contents,
    ResultCallback result_callback) {
  if (install_options_.only_use_app_info_factory) {
    DCHECK(install_options_.app_info_factory);
    InstallFromInfo(std::move(result_callback));
    return;
  }

  url_loader_->PrepareForLoad(
      web_contents,
      base::BindOnce(&ExternallyManagedAppInstallTask::OnWebContentsReady,
                     weak_ptr_factory_.GetWeakPtr(), web_contents,
                     std::move(result_callback)));
}

void ExternallyManagedAppInstallTask::OnWebContentsReady(
    content::WebContents* web_contents,
    ResultCallback result_callback,
    WebAppUrlLoader::Result prepare_for_load_result) {
  // TODO(crbug.com/1098139): Handle the scenario where WebAppUrlLoader fails to
  // load about:blank and flush WebContents states.
  url_loader_->LoadUrl(
      install_options_.install_url, web_contents,
      WebAppUrlLoader::UrlComparison::kSameOrigin,
      base::BindOnce(&ExternallyManagedAppInstallTask::OnUrlLoaded,
                     weak_ptr_factory_.GetWeakPtr(), web_contents,
                     std::move(result_callback)));
}

void ExternallyManagedAppInstallTask::OnUrlLoaded(
    content::WebContents* web_contents,
    ResultCallback result_callback,
    WebAppUrlLoader::Result load_url_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(web_contents->GetBrowserContext(), profile_);

  ResultCallback retry_on_failure = base::BindOnce(
      &ExternallyManagedAppInstallTask::TryAppInfoFactoryOnFailure,
      weak_ptr_factory_.GetWeakPtr(), std::move(result_callback));

  if (load_url_result == WebAppUrlLoader::Result::kUrlLoaded) {
    // If we are not re-installing a placeholder, then no need to uninstall
    // anything.
    if (!install_options_.reinstall_placeholder) {
      ContinueWebAppInstall(web_contents, std::move(retry_on_failure));
      return;
    }
    // Calling InstallWebAppWithOptions with the same URL used to install a
    // placeholder won't necessarily replace the placeholder app, because the
    // new app might be installed with a new AppId. To avoid this, always
    // uninstall the placeholder app.
    UninstallPlaceholderApp(web_contents, std::move(retry_on_failure));
    return;
  }

  if (install_options_.install_placeholder) {
    InstallPlaceholder(std::move(retry_on_failure));
    return;
  }

  // Avoid counting an error if we are shutting down. This matches later
  // stages of install where if the WebContents is destroyed we return early.
  if (load_url_result == WebAppUrlLoader::Result::kFailedWebContentsDestroyed)
    return;

  InstallResultCode code = InstallResultCode::kInstallURLLoadFailed;

  switch (load_url_result) {
    case WebAppUrlLoader::Result::kUrlLoaded:
    case WebAppUrlLoader::Result::kFailedWebContentsDestroyed:
      // Handled above.
      NOTREACHED();
      break;
    case WebAppUrlLoader::Result::kRedirectedUrlLoaded:
      code = InstallResultCode::kInstallURLRedirected;
      break;
    case WebAppUrlLoader::Result::kFailedUnknownReason:
      code = InstallResultCode::kInstallURLLoadFailed;
      break;
    case WebAppUrlLoader::Result::kFailedPageTookTooLong:
      code = InstallResultCode::kInstallURLLoadTimeOut;
      break;
    case WebAppUrlLoader::Result::kFailedErrorPageLoaded:
      code = InstallResultCode::kInstallURLLoadFailed;
      break;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(retry_on_failure),
                     ExternallyManagedAppManager::InstallResult(code)));
}

void ExternallyManagedAppInstallTask::InstallFromInfo(
    ResultCallback result_callback) {
  auto internal_install_source = ConvertExternalInstallSourceToInstallSource(
      install_options().install_source);
  auto install_params = ConvertExternalInstallOptionsToParams(install_options_);
  auto web_app_info = install_options_.app_info_factory.Run();
  for (std::string& search_term : install_params.additional_search_terms) {
    web_app_info->additional_search_terms.push_back(std::move(search_term));
  }
  install_manager_->InstallWebAppFromInfo(
      std::move(web_app_info),
      /*overwrite_existing_manifest_fields=*/install_params.force_reinstall,
      ForInstallableSite::kYes, install_params, internal_install_source,
      base::BindOnce(&ExternallyManagedAppInstallTask::OnWebAppInstalled,
                     weak_ptr_factory_.GetWeakPtr(), /* is_placeholder=*/false,
                     /*offline_install=*/true, std::move(result_callback)));
}

void ExternallyManagedAppInstallTask::UninstallPlaceholderApp(
    content::WebContents* web_contents,
    ResultCallback result_callback) {
  absl::optional<AppId> app_id =
      externally_installed_app_prefs_.LookupPlaceholderAppId(
          install_options_.install_url);

  // If there is no placeholder app or the app is not installed,
  // then no need to uninstall anything.
  if (!app_id.has_value() || !registrar_->IsInstalled(app_id.value())) {
    ContinueWebAppInstall(web_contents, std::move(result_callback));
    return;
  }

  // Otherwise, uninstall the placeholder app.
  install_finalizer_->UninstallExternalWebAppByUrl(
      install_options_.install_url,
      webapps::WebappUninstallSource::kPlaceholderReplacement,
      base::BindOnce(&ExternallyManagedAppInstallTask::OnPlaceholderUninstalled,
                     weak_ptr_factory_.GetWeakPtr(), web_contents,
                     std::move(result_callback)));
}

void ExternallyManagedAppInstallTask::OnPlaceholderUninstalled(
    content::WebContents* web_contents,
    ResultCallback result_callback,
    bool uninstalled) {
  if (!uninstalled) {
    LOG(ERROR) << "Failed to uninstall placeholder for: "
               << install_options_.install_url;
    std::move(result_callback)
        .Run(ExternallyManagedAppManager::InstallResult(
            InstallResultCode::kFailedPlaceholderUninstall));
    return;
  }
  ContinueWebAppInstall(web_contents, std::move(result_callback));
}

void ExternallyManagedAppInstallTask::ContinueWebAppInstall(
    content::WebContents* web_contents,
    ResultCallback result_callback) {
  auto install_params = ConvertExternalInstallOptionsToParams(install_options_);
  auto install_source = ConvertExternalInstallSourceToInstallSource(
      install_options_.install_source);

  install_manager_->InstallWebAppWithParams(
      web_contents, install_params, install_source,
      base::BindOnce(&ExternallyManagedAppInstallTask::OnWebAppInstalled,
                     weak_ptr_factory_.GetWeakPtr(), /*is_placeholder=*/false,
                     /*offline_install=*/false, std::move(result_callback)));
}

void ExternallyManagedAppInstallTask::InstallPlaceholder(
    ResultCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  absl::optional<AppId> app_id =
      externally_installed_app_prefs_.LookupPlaceholderAppId(
          install_options_.install_url);
  if (app_id.has_value() && registrar_->IsInstalled(app_id.value())) {
    // No need to install a placeholder app again.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       ExternallyManagedAppManager::InstallResult(
                           InstallResultCode::kSuccessNewInstall, app_id)));
    return;
  }

  WebApplicationInfo web_app_info;
  web_app_info.title =
      install_options_.placeholder_name
          ? base::UTF8ToUTF16(install_options_.placeholder_name.value())
          : install_options_.fallback_app_name
                ? base::UTF8ToUTF16(install_options_.fallback_app_name.value())
                : base::UTF8ToUTF16(install_options_.install_url.spec());
  web_app_info.start_url = install_options_.install_url;

  web_app_info.user_display_mode = install_options_.user_display_mode;

  WebAppInstallFinalizer::FinalizeOptions options;
  options.install_source = ConvertExternalInstallSourceToInstallSource(
      install_options_.install_source);
  options.overwrite_existing_manifest_fields = false;

  install_finalizer_->FinalizeInstall(
      web_app_info, options,
      base::BindOnce(&ExternallyManagedAppInstallTask::OnWebAppInstalled,
                     weak_ptr_factory_.GetWeakPtr(), /*is_placeholder=*/true,
                     /*offline_install=*/false, std::move(callback)));
}

void ExternallyManagedAppInstallTask::OnWebAppInstalled(
    bool is_placeholder,
    bool offline_install,
    ResultCallback result_callback,
    const AppId& app_id,
    InstallResultCode code) {
  if (!IsNewInstall(code)) {
    std::move(result_callback)
        .Run(ExternallyManagedAppManager::InstallResult(code));
    return;
  }

  bool uninstall_and_replace_triggered =
      ui_manager_->UninstallAndReplaceIfExists(
          install_options().uninstall_and_replace, app_id);

  externally_installed_app_prefs_.Insert(install_options_.install_url, app_id,
                                         install_options_.install_source);
  externally_installed_app_prefs_.SetIsPlaceholder(install_options_.install_url,
                                                   is_placeholder);

  if (offline_install) {
    code = install_options().only_use_app_info_factory
               ? InstallResultCode::kSuccessOfflineOnlyInstall
               : InstallResultCode::kSuccessOfflineFallbackInstall;
  }
  base::ScopedClosureRunner scoped_closure(
      base::BindOnce(std::move(result_callback),
                     ExternallyManagedAppManager::InstallResult(
                         code, app_id, uninstall_and_replace_triggered)));

  if (!is_placeholder) {
    registrar_->NotifyWebAppInstalledWithOsHooks(app_id);
    return;
  }
  InstallOsHooksOptions options;
  options.os_hooks[OsHookType::kShortcuts] =
      install_options_.add_to_applications_menu;
  options.os_hooks[OsHookType::kShortcutsMenu] =
      install_options_.add_to_applications_menu;
  options.add_to_desktop = install_options_.add_to_desktop;
  options.add_to_quick_launch_bar = install_options_.add_to_quick_launch_bar;
  options.os_hooks[OsHookType::kRunOnOsLogin] =
      install_options_.run_on_os_login;

  // TODO(crbug.com/1087219): Determine if |register_file_handlers| should be
  // configured from somewhere else rather than always true.
  options.os_hooks[OsHookType::kFileHandlers] = true;
  options.os_hooks[OsHookType::kProtocolHandlers] = true;

  const WebApp* web_app = registrar_->GetAppById(app_id);
  options.os_hooks[OsHookType::kUninstallationViaOsSettings] =
      web_app->CanUserUninstallWebApp();
#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS))
  options.os_hooks[OsHookType::kUrlHandlers] = true;
#else
  options.os_hooks[OsHookType::kUrlHandlers] = false;
#endif

  MaybeDisableOsIntegration(registrar_, app_id, &options);

  os_integration_manager_->InstallOsHooks(
      app_id,
      base::BindOnce(&ExternallyManagedAppInstallTask::OnOsHooksCreated,
                     weak_ptr_factory_.GetWeakPtr(), app_id,
                     std::move(scoped_closure)),
      nullptr, options);
}

void ExternallyManagedAppInstallTask::OnOsHooksCreated(
    const AppId& app_id,
    base::ScopedClosureRunner scoped_closure,
    const OsHooksErrors os_hooks_errors) {
  registrar_->NotifyWebAppInstalledWithOsHooks(app_id);
  scoped_closure.RunAndReset();
}

void ExternallyManagedAppInstallTask::TryAppInfoFactoryOnFailure(
    ResultCallback result_callback,
    ExternallyManagedAppManager::InstallResult result) {
  if (!IsSuccess(result.code) && install_options().app_info_factory) {
    InstallFromInfo(std::move(result_callback));
    return;
  }
  std::move(result_callback).Run(std::move(result));
}

}  // namespace web_app
