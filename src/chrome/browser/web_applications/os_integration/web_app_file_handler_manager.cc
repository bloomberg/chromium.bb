// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_file_handler_manager.h"

#include <set>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_registration.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace web_app {

namespace {

// Used to enable running tests on platforms that don't support file handling
// icons.
absl::optional<bool> g_icons_supported_by_os_override;

base::RepeatingCallback<void(bool)>& GetOsIntegrationCallback() {
  static base::NoDestructor<base::RepeatingCallback<void(bool)>> instance;
  return *instance;
}

}  // namespace

WebAppFileHandlerManager::WebAppFileHandlerManager(Profile* profile)
    : profile_(profile) {}

WebAppFileHandlerManager::~WebAppFileHandlerManager() = default;

void WebAppFileHandlerManager::SetSubsystems(WebAppSyncBridge* sync_bridge) {
  sync_bridge_ = sync_bridge;
}

void WebAppFileHandlerManager::Start() {
  DCHECK(sync_bridge_);

  // Don't compete for resources during browser startup.
  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(
          FROM_HERE,
          base::BindOnce(base::IgnoreResult(
                             &WebAppFileHandlerManager::SyncOsIntegrationState),
                         weak_ptr_factory_.GetWeakPtr()));
}

// static
void WebAppFileHandlerManager::DisableOsIntegrationForTesting(
    const base::RepeatingCallback<void(bool)>& set_os_integration) {
  GetOsIntegrationCallback() = set_os_integration;
}

// static
void WebAppFileHandlerManager::SetIconsSupportedByOsForTesting(bool value) {
  g_icons_supported_by_os_override = value;
}

void WebAppFileHandlerManager::EnableAndRegisterOsFileHandlers(
    const AppId& app_id) {
  if (!IsFileHandlingAPIAvailable(app_id))
    return;

  SetOsIntegrationState(app_id, OsIntegrationState::kEnabled);

  if (GetOsIntegrationCallback()) {
    GetOsIntegrationCallback().Run(true);
    return;
  }

  if (!ShouldRegisterFileHandlersWithOs())
    return;

#if !BUILDFLAG(IS_MAC)
  // File handler registration is done via shortcuts creation on MacOS,
  // WebAppShortcutManager::BuildShortcutInfoForWebApp collects file handler
  // information to shortcut_info->file_handler_extensions, then used by MacOS
  // implementation of |internals::CreatePlatformShortcuts|. So we avoid
  // creating shortcuts twice here.
  const apps::FileHandlers* file_handlers = GetEnabledFileHandlers(app_id);
  if (file_handlers) {
    RegisterFileHandlersWithOs(app_id, GetRegistrar()->GetAppShortName(app_id),
                               profile_, *file_handlers);
  }
#endif
}

void WebAppFileHandlerManager::DisableAndUnregisterOsFileHandlers(
    const AppId& app_id,
    ResultCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!ShouldOsIntegrationBeEnabled(app_id)) {
    // No work is required.
    std::move(callback).Run(Result::kOk);
    return;
  }

  SetOsIntegrationState(app_id, OsIntegrationState::kDisabled);

  if (GetOsIntegrationCallback()) {
    GetOsIntegrationCallback().Run(false);
    std::move(callback).Run(Result::kOk);
    return;
  }

  if (!ShouldRegisterFileHandlersWithOs()) {
    // This enumeration signals if there was not an error. Exiting early here is
    // WAI, so this is a success.
    std::move(callback).Run(Result::kOk);
    return;
  }

  // File handler information is embedded in the shortcut, when
  // |DeleteSharedAppShims| is called in
  // |OsIntegrationManager::UninstallOsHooks|, file handlers are also
  // unregistered.
#if BUILDFLAG(IS_MAC)
  // When updating file handlers, |callback| here triggers the registering of
  // the new file handlers. It is therefore important that |callback| not be
  // dropped on the floor.
  // https://crbug.com/1201993
  std::move(callback).Run(Result::kOk);
#else
  UnregisterFileHandlersWithOs(app_id, profile_, std::move(callback));
#endif
}

const apps::FileHandlers* WebAppFileHandlerManager::GetEnabledFileHandlers(
    const AppId& app_id) const {
  if (ShouldOsIntegrationBeEnabled(app_id) &&
      IsFileHandlingAPIAvailable(app_id) &&
      !GetRegistrar()->IsAppFileHandlerPermissionBlocked(app_id)) {
    return GetAllFileHandlers(app_id);
  }

  return nullptr;
}

bool WebAppFileHandlerManager::IsFileHandlingAPIAvailable(
    const AppId& app_id) const {
  if (base::FeatureList::IsEnabled(blink::features::kFileHandlingAPI))
    return true;

  // May be null in unit tests.
  if (GetRegistrar()) {
    const WebApp* web_app = GetRegistrar()->GetAppById(app_id);
    return web_app && web_app->IsSystemApp();
  }

  return false;
}

// static
bool WebAppFileHandlerManager::IconsEnabled() {
  return g_icons_supported_by_os_override.value_or(
             FileHandlingIconsSupportedByOs()) &&
         base::FeatureList::IsEnabled(blink::features::kFileHandlingIcons);
}

void WebAppFileHandlerManager::SyncOsIntegrationStateForTesting() {
  SyncOsIntegrationState();
}

const apps::FileHandlers* WebAppFileHandlerManager::GetAllFileHandlers(
    const AppId& app_id) const {
  const WebApp* web_app = GetRegistrar()->GetAppById(app_id);
  return web_app && !web_app->file_handlers().empty()
             ? &web_app->file_handlers()
             : nullptr;
}

WebAppFileHandlerManager::LaunchInfos
WebAppFileHandlerManager::GetMatchingFileHandlerUrls(
    const AppId& app_id,
    const std::vector<base::FilePath>& launch_files) {
  LaunchInfos launch_infos;
  if (!IsFileHandlingAPIAvailable(app_id) || launch_files.empty() ||
      GetRegistrar()->IsAppFileHandlerPermissionBlocked(app_id)) {
    return launch_infos;
  }

  std::map<const apps::FileHandler*, std::vector<base::FilePath>>
      launch_handlers;

  const apps::FileHandlers* file_handlers = GetAllFileHandlers(app_id);
  if (!file_handlers)
    return launch_infos;

  for (const auto& file_path : launch_files) {
    std::string file_extension =
        base::FilePath(file_path.Extension()).AsUTF8Unsafe();
    if (file_extension.length() <= 1)
      continue;

    for (const auto& file_handler : *file_handlers) {
      std::set<std::string> supported_file_extensions =
          apps::GetFileExtensionsFromFileHandlers({file_handler});
      if (base::Contains(supported_file_extensions, file_extension)) {
        launch_handlers[&file_handler].push_back(file_path);
        break;
      }
    }
  }

  for (auto& launch_handler : launch_handlers) {
    const GURL& action = launch_handler.first->action;
    if (launch_handler.first->launch_type ==
        apps::FileHandler::LaunchType::kMultipleClients) {
      for (base::FilePath& file : launch_handler.second) {
        launch_infos.emplace_back(action,
                                  std::vector<base::FilePath>{std::move(file)});
      }
    } else {
      launch_infos.emplace_back(action, std::move(launch_handler.second));
    }
  }

  return launch_infos;
}

void WebAppFileHandlerManager::SetOsIntegrationState(
    const AppId& app_id,
    OsIntegrationState os_state) {
  ScopedRegistryUpdate update(sync_bridge_);
  update->UpdateApp(app_id)->SetFileHandlerOsIntegrationState(os_state);
}

bool WebAppFileHandlerManager::ShouldOsIntegrationBeEnabled(
    const AppId& app_id) const {
  return !ShouldRegisterFileHandlersWithOs() ||
         (GetRegistrar() &&
          GetRegistrar()->ExpectThatFileHandlersAreRegisteredWithOs(app_id));
}

const WebAppRegistrar* WebAppFileHandlerManager::GetRegistrar() const {
  return sync_bridge_ ? &sync_bridge_->registrar() : nullptr;
}

void WebAppFileHandlerManager::SyncOsIntegrationState() {
  if (GetRegistrar()) {
    for (AppId& id : GetRegistrar()->GetAppIds()) {
      UpdateFileHandlerOsIntegration(
          WebAppProvider::GetForLocalAppsUnchecked(profile_), id,
          base::DoNothing());
    }
  }
}

}  // namespace web_app
