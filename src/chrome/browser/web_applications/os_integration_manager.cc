// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration_manager.h"

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_shortcut.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_uninstallation_via_os_settings_registration.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/web_applications/app_shim_registry_mac.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace {
bool g_suppress_os_hooks_for_testing_ = false;
}  // namespace

namespace web_app {

OsIntegrationManager::ScopedSuppressForTesting::ScopedSuppressForTesting()
    :
// Creating OS hooks on ChromeOS doesn't write files to disk, so it's
// unnecessary to suppress and it provides better crash coverage.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
      scope_(&g_suppress_os_hooks_for_testing_, true)
#else
      scope_(&g_suppress_os_hooks_for_testing_, false)
#endif
{
}

OsIntegrationManager::ScopedSuppressForTesting::~ScopedSuppressForTesting() =
    default;

// This barrier is designed to accumulate errors from calls to OS hook
// operations, and call the completion callback when all OS hook operations
// have completed. The |callback| is called when all copies of this object and
// all callbacks created using this object are destroyed.
class OsIntegrationManager::OsHooksBarrier
    : public base::RefCounted<OsHooksBarrier> {
 public:
  explicit OsHooksBarrier(OsHooksErrors errors_default,
                          InstallOsHooksCallback callback)
      : errors_(errors_default), callback_(std::move(callback)) {}

  void OnError(OsHookType::Type type) { AddResult(type, Result::kError); }

  ResultCallback CreateBarrierCallbackForType(OsHookType::Type type) {
    return base::BindOnce(&OsHooksBarrier::AddResult, this, type);
  }

 private:
  friend class base::RefCounted<OsHooksBarrier>;

  ~OsHooksBarrier() {
    DCHECK(callback_);
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), std::move(errors_)));
  }

  void AddResult(OsHookType::Type type, Result result) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    errors_[type] = result == Result::kError ? true : false;
  }

  OsHooksErrors errors_;
  InstallOsHooksCallback callback_;
};

InstallOsHooksOptions::InstallOsHooksOptions() = default;
InstallOsHooksOptions::InstallOsHooksOptions(
    const InstallOsHooksOptions& other) = default;
InstallOsHooksOptions& InstallOsHooksOptions::operator=(
    const InstallOsHooksOptions& other) = default;

OsIntegrationManager::OsIntegrationManager(
    Profile* profile,
    std::unique_ptr<WebAppShortcutManager> shortcut_manager,
    std::unique_ptr<WebAppFileHandlerManager> file_handler_manager,
    std::unique_ptr<WebAppProtocolHandlerManager> protocol_handler_manager,
    std::unique_ptr<UrlHandlerManager> url_handler_manager)
    : profile_(profile),
      shortcut_manager_(std::move(shortcut_manager)),
      file_handler_manager_(std::move(file_handler_manager)),
      protocol_handler_manager_(std::move(protocol_handler_manager)),
      url_handler_manager_(std::move(url_handler_manager)) {}

OsIntegrationManager::~OsIntegrationManager() = default;

void OsIntegrationManager::SetSubsystems(WebAppSyncBridge* sync_bridge,
                                         WebAppRegistrar* registrar,
                                         WebAppUiManager* ui_manager,
                                         WebAppIconManager* icon_manager) {
  // TODO(estade): fetch the registrar from `sync_bridge` instead of passing
  // both as arguments.
  registrar_ = registrar;
  ui_manager_ = ui_manager;
  file_handler_manager_->SetSubsystems(sync_bridge);
  shortcut_manager_->SetSubsystems(icon_manager, registrar);
  if (protocol_handler_manager_)
    protocol_handler_manager_->SetSubsystems(registrar);
  if (url_handler_manager_)
    url_handler_manager_->SetSubsystems(registrar);
}

void OsIntegrationManager::Start() {
  DCHECK(registrar_);
  DCHECK(file_handler_manager_);

  registrar_observation_.Observe(registrar_.get());

#if BUILDFLAG(IS_MAC)
  // Ensure that all installed apps are included in the AppShimRegistry when the
  // profile is loaded. This is redundant, because apps are registered when they
  // are installed. It is necessary, however, because app registration was added
  // long after app installation launched. This should be removed after shipping
  // for a few versions (whereupon it may be assumed that most applications have
  // been registered).
  std::vector<AppId> app_ids = registrar_->GetAppIds();
  for (const auto& app_id : app_ids) {
    AppShimRegistry::Get()->OnAppInstalledForProfile(app_id,
                                                     profile_->GetPath());
  }
#endif
  file_handler_manager_->Start();
  if (protocol_handler_manager_)
    protocol_handler_manager_->Start();
}

void OsIntegrationManager::InstallOsHooks(
    const AppId& app_id,
    InstallOsHooksCallback callback,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    InstallOsHooksOptions options) {
  if (g_suppress_os_hooks_for_testing_) {
    OsHooksErrors os_hooks_errors;
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), os_hooks_errors));
    return;
  }
  MacAppShimOnAppInstalledForProfile(app_id);

  OsHooksErrors os_hooks_errors;
  scoped_refptr<OsHooksBarrier> barrier = base::MakeRefCounted<OsHooksBarrier>(
      os_hooks_errors, std::move(callback));

  DCHECK(options.os_hooks[OsHookType::kShortcuts] ||
         !options.os_hooks[OsHookType::kShortcutsMenu])
      << "Cannot install shortcuts menu without installing shortcuts.";

  auto shortcuts_callback = base::BindOnce(
      &OsIntegrationManager::OnShortcutsCreated, weak_ptr_factory_.GetWeakPtr(),
      app_id, std::move(web_app_info), options, barrier);

#if BUILDFLAG(IS_MAC)
  // This has to happen before creating shortcuts on Mac because the shortcut
  // creation step uses the file type associations which are marked for enabling
  // by `RegisterFileHandlers()`.
  if (options.os_hooks[OsHookType::kFileHandlers]) {
    RegisterFileHandlers(app_id, barrier->CreateBarrierCallbackForType(
                                     OsHookType::kFileHandlers));
  }
#endif

  // TODO(ortuno): Make adding a shortcut to the applications menu independent
  // from adding a shortcut to desktop.
  if (options.os_hooks[OsHookType::kShortcuts]) {
    CreateShortcuts(app_id, options.add_to_desktop,
                    std::move(shortcuts_callback));
  } else {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(shortcuts_callback),
                                  /*shortcuts_created=*/false));
  }
}

void OsIntegrationManager::UninstallAllOsHooks(
    const AppId& app_id,
    UninstallOsHooksCallback callback) {
  OsHooksOptions os_hooks;
  os_hooks.set();
  UninstallOsHooks(app_id, os_hooks, std::move(callback));
}

void OsIntegrationManager::UninstallOsHooks(const AppId& app_id,
                                            const OsHooksOptions& os_hooks,
                                            UninstallOsHooksCallback callback) {
  if (g_suppress_os_hooks_for_testing_) {
    OsHooksErrors os_hooks_errors;
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), os_hooks_errors));
    return;
  }

  OsHooksErrors os_hooks_errors;
  scoped_refptr<OsHooksBarrier> barrier = base::MakeRefCounted<OsHooksBarrier>(
      os_hooks_errors, std::move(callback));

  if (os_hooks[OsHookType::kShortcutsMenu]) {
    bool success = UnregisterShortcutsMenu(app_id);
    if (!success)
      barrier->OnError(OsHookType::kShortcutsMenu);
  }

  if (os_hooks[OsHookType::kShortcuts] || os_hooks[OsHookType::kRunOnOsLogin]) {
    std::unique_ptr<ShortcutInfo> shortcut_info = BuildShortcutInfo(app_id);
    base::FilePath shortcut_data_dir =
        internals::GetShortcutDataDir(*shortcut_info);

    if (os_hooks[OsHookType::kRunOnOsLogin] &&
        base::FeatureList::IsEnabled(features::kDesktopPWAsRunOnOsLogin)) {
      UnregisterRunOnOsLogin(
          app_id, shortcut_info->profile_path, shortcut_info->title,
          barrier->CreateBarrierCallbackForType(OsHookType::kRunOnOsLogin));
    }

    if (os_hooks[OsHookType::kShortcuts]) {
      DeleteShortcuts(
          app_id, shortcut_data_dir, std::move(shortcut_info),
          barrier->CreateBarrierCallbackForType(OsHookType::kShortcuts));
    }
  }
  // unregistration and record errors during unregistration.
  if (os_hooks[OsHookType::kFileHandlers]) {
    UnregisterFileHandlers(app_id, barrier->CreateBarrierCallbackForType(
                                       OsHookType::kFileHandlers));
  }

  if (os_hooks[OsHookType::kProtocolHandlers]) {
    UnregisterProtocolHandlers(app_id, barrier->CreateBarrierCallbackForType(
                                           OsHookType::kProtocolHandlers));
  }

  if (os_hooks[OsHookType::kUrlHandlers])
    UnregisterUrlHandlers(app_id);

  // There is a chance uninstallation point was created with feature flag
  // enabled so we need to clean it up regardless of feature flag state.
  if (os_hooks[OsHookType::kUninstallationViaOsSettings])
    UnregisterWebAppOsUninstallation(app_id);
}

void OsIntegrationManager::UpdateOsHooks(
    const AppId& app_id,
    base::StringPiece old_name,
    FileHandlerUpdateAction file_handlers_need_os_update,
    const WebAppInstallInfo& web_app_info,
    UpdateOsHooksCallback callback) {
  if (g_suppress_os_hooks_for_testing_) {
    OsHooksErrors os_hooks_errors;
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), os_hooks_errors));
    return;
  }

  OsHooksErrors os_hooks_errors;
  scoped_refptr<OsHooksBarrier> barrier = base::MakeRefCounted<OsHooksBarrier>(
      os_hooks_errors, std::move(callback));

  UpdateFileHandlers(app_id, file_handlers_need_os_update,
                     base::BindOnce(barrier->CreateBarrierCallbackForType(
                         OsHookType::kFileHandlers)));
  UpdateShortcuts(app_id, old_name,
                  base::BindOnce(barrier->CreateBarrierCallbackForType(
                                     OsHookType::kShortcuts),
                                 Result::kOk));
  UpdateShortcutsMenu(app_id, web_app_info);
  UpdateUrlHandlers(
      app_id,
      base::BindOnce(
          [](ResultCallback callback, bool success) {
            std::move(callback).Run(success ? Result::kOk : Result::kError);
          },
          barrier->CreateBarrierCallbackForType(OsHookType::kUrlHandlers)));
  UpdateProtocolHandlers(app_id, /*force_shortcut_updates_if_needed=*/false,
                         base::BindOnce(barrier->CreateBarrierCallbackForType(
                                            OsHookType::kProtocolHandlers),
                                        Result::kOk));
}

void OsIntegrationManager::GetAppExistingShortCutLocation(
    ShortcutLocationCallback callback,
    std::unique_ptr<ShortcutInfo> shortcut_info) {
  DCHECK(shortcut_manager_);
  shortcut_manager_->GetAppExistingShortCutLocation(std::move(callback),
                                                    std::move(shortcut_info));
}

void OsIntegrationManager::GetShortcutInfoForApp(
    const AppId& app_id,
    WebAppShortcutManager::GetShortcutInfoCallback callback) {
  DCHECK(shortcut_manager_);
  return shortcut_manager_->GetShortcutInfoForApp(app_id, std::move(callback));
}

bool OsIntegrationManager::IsFileHandlingAPIAvailable(const AppId& app_id) {
  DCHECK(file_handler_manager_);
  return file_handler_manager_->IsFileHandlingAPIAvailable(app_id);
}

const apps::FileHandlers* OsIntegrationManager::GetEnabledFileHandlers(
    const AppId& app_id) const {
  DCHECK(file_handler_manager_);
  return file_handler_manager_->GetEnabledFileHandlers(app_id);
}

const absl::optional<GURL> OsIntegrationManager::GetMatchingFileHandlerURL(
    const AppId& app_id,
    const std::vector<base::FilePath>& launch_files) {
  DCHECK(file_handler_manager_);
  return file_handler_manager_->GetMatchingFileHandlerURL(app_id, launch_files);
}

absl::optional<GURL> OsIntegrationManager::TranslateProtocolUrl(
    const AppId& app_id,
    const GURL& protocol_url) {
  if (!protocol_handler_manager_)
    return absl::optional<GURL>();

  return protocol_handler_manager_->TranslateProtocolUrl(app_id, protocol_url);
}

std::vector<ProtocolHandler> OsIntegrationManager::GetHandlersForProtocol(
    const std::string& protocol) {
  if (!protocol_handler_manager_)
    return std::vector<ProtocolHandler>();

  return protocol_handler_manager_->GetHandlersFor(protocol);
}

std::vector<ProtocolHandler> OsIntegrationManager::GetAppProtocolHandlers(
    const AppId& app_id) {
  if (!protocol_handler_manager_)
    return std::vector<ProtocolHandler>();

  return protocol_handler_manager_->GetAppProtocolHandlers(app_id);
}

std::vector<ProtocolHandler>
OsIntegrationManager::GetAllowedHandlersForProtocol(
    const std::string& protocol) {
  if (!protocol_handler_manager_)
    return std::vector<ProtocolHandler>();

  return protocol_handler_manager_->GetAllowedHandlersForProtocol(protocol);
}

std::vector<ProtocolHandler>
OsIntegrationManager::GetDisallowedHandlersForProtocol(
    const std::string& protocol) {
  if (!protocol_handler_manager_)
    return std::vector<ProtocolHandler>();

  return protocol_handler_manager_->GetDisallowedHandlersForProtocol(protocol);
}

WebAppFileHandlerManager&
OsIntegrationManager::file_handler_manager_for_testing() {
  DCHECK(file_handler_manager_);
  return *file_handler_manager_;
}

UrlHandlerManager& OsIntegrationManager::url_handler_manager_for_testing() {
  DCHECK(url_handler_manager_);
  return *url_handler_manager_;
}

WebAppProtocolHandlerManager&
OsIntegrationManager::protocol_handler_manager_for_testing() {
  DCHECK(protocol_handler_manager_);
  return *protocol_handler_manager_;
}

FakeOsIntegrationManager* OsIntegrationManager::AsTestOsIntegrationManager() {
  return nullptr;
}

void OsIntegrationManager::CreateShortcuts(const AppId& app_id,
                                           bool add_to_desktop,
                                           CreateShortcutsCallback callback) {
  if (shortcut_manager_->CanCreateShortcuts()) {
    shortcut_manager_->CreateShortcuts(app_id, add_to_desktop,
                                       std::move(callback));
  } else {
    std::move(callback).Run(false);
  }
}

void OsIntegrationManager::RegisterFileHandlers(const AppId& app_id,
                                                ResultCallback callback) {
  DCHECK(file_handler_manager_);
  file_handler_manager_->EnableAndRegisterOsFileHandlers(app_id);

  // TODO(crbug.com/1087219): callback should be run after all hooks are
  // deployed, need to refactor filehandler to allow this.
  std::move(callback).Run(Result::kOk);
}

void OsIntegrationManager::RegisterProtocolHandlers(const AppId& app_id,
                                                    ResultCallback callback) {
  // Disable protocol handler unregistration on Win7 due to bad interactions
  // between preinstalled app scenarios and the need for elevation to unregister
  // protocol handlers on that platform. See crbug.com/1224327 for context.
#if BUILDFLAG(IS_WIN)
  if (base::win::GetVersion() == base::win::Version::WIN7) {
    std::move(callback).Run(Result::kOk);
    return;
  }
#endif  // BUILDFLAG(IS_WIN)

  if (!protocol_handler_manager_) {
    std::move(callback).Run(Result::kOk);
    return;
  }

  protocol_handler_manager_->RegisterOsProtocolHandlers(app_id,
                                                        std::move(callback));
}

void OsIntegrationManager::RegisterUrlHandlers(const AppId& app_id,
                                               ResultCallback callback) {
  if (!url_handler_manager_) {
    std::move(callback).Run(Result::kOk);
    return;
  }

  url_handler_manager_->RegisterUrlHandlers(app_id, std::move(callback));
}

void OsIntegrationManager::RegisterShortcutsMenu(
    const AppId& app_id,
    const std::vector<WebAppShortcutsMenuItemInfo>& shortcuts_menu_item_infos,
    const ShortcutsMenuIconBitmaps& shortcuts_menu_icon_bitmaps,
    ResultCallback callback) {
  if (!ShouldRegisterShortcutsMenuWithOs()) {
    std::move(callback).Run(Result::kOk);
    return;
  }

  DCHECK(shortcut_manager_);
  shortcut_manager_->RegisterShortcutsMenuWithOs(
      app_id, shortcuts_menu_item_infos, shortcuts_menu_icon_bitmaps);

  // TODO(https://crbug.com/1098471): fix RegisterShortcutsMenuWithOs to
  // take callback.
  std::move(callback).Run(Result::kOk);
}

void OsIntegrationManager::ReadAllShortcutsMenuIconsAndRegisterShortcutsMenu(
    const AppId& app_id,
    ResultCallback callback) {
  if (!ShouldRegisterShortcutsMenuWithOs()) {
    std::move(callback).Run(Result::kOk);
    return;
  }

  shortcut_manager_->ReadAllShortcutsMenuIconsAndRegisterShortcutsMenu(
      app_id, std::move(callback));
}

void OsIntegrationManager::RegisterRunOnOsLogin(
    const AppId& app_id,
    RegisterRunOnOsLoginCallback callback) {
  GetShortcutInfoForApp(
      app_id,
      base::BindOnce(
          &OsIntegrationManager::OnShortcutInfoRetrievedRegisterRunOnOsLogin,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void OsIntegrationManager::MacAppShimOnAppInstalledForProfile(
    const AppId& app_id) {
#if BUILDFLAG(IS_MAC)
  AppShimRegistry::Get()->OnAppInstalledForProfile(app_id, profile_->GetPath());
#endif
}

void OsIntegrationManager::AddAppToQuickLaunchBar(const AppId& app_id) {
  DCHECK(ui_manager_);
  if (ui_manager_->CanAddAppToQuickLaunchBar()) {
    ui_manager_->AddAppToQuickLaunchBar(app_id);
  }
}

void OsIntegrationManager::RegisterWebAppOsUninstallation(
    const AppId& app_id,
    const std::string& name) {
  if (ShouldRegisterUninstallationViaOsSettingsWithOs()) {
    RegisterUninstallationViaOsSettingsWithOs(app_id, name, profile_);
  }
}

bool OsIntegrationManager::UnregisterShortcutsMenu(const AppId& app_id) {
  if (!ShouldRegisterShortcutsMenuWithOs())
    return true;
  return UnregisterShortcutsMenuWithOs(app_id, profile_->GetPath());
}

void OsIntegrationManager::UnregisterRunOnOsLogin(
    const AppId& app_id,
    const base::FilePath& profile_path,
    const std::u16string& shortcut_title,
    UnregisterRunOnOsLoginCallback callback) {
  ScheduleUnregisterRunOnOsLogin(app_id, profile_path, shortcut_title,
                                 std::move(callback));
}

void OsIntegrationManager::DeleteShortcuts(
    const AppId& app_id,
    const base::FilePath& shortcuts_data_dir,
    std::unique_ptr<ShortcutInfo> shortcut_info,
    ResultCallback callback) {
  if (shortcut_manager_->CanCreateShortcuts()) {
    auto shortcuts_callback = base::BindOnce(
        &OsIntegrationManager::OnShortcutsDeleted,
        weak_ptr_factory_.GetWeakPtr(), app_id, std::move(callback));

    shortcut_manager_->DeleteShortcuts(app_id, shortcuts_data_dir,
                                       std::move(shortcut_info),
                                       std::move(shortcuts_callback));
  } else {
    std::move(callback).Run(Result::kOk);
  }
}

void OsIntegrationManager::UnregisterFileHandlers(const AppId& app_id,
                                                  ResultCallback callback) {
  DCHECK(file_handler_manager_);

  file_handler_manager_->DisableAndUnregisterOsFileHandlers(
      app_id, std::move(callback));
}

void OsIntegrationManager::UnregisterProtocolHandlers(const AppId& app_id,
                                                      ResultCallback callback) {
  // Disable protocol handler unregistration on Win7 due to bad interactions
  // between preinstalled app scenarios and the need for elevation to unregister
  // protocol handlers on that platform. See crbug.com/1224327 for context.
#if BUILDFLAG(IS_WIN)
  if (base::win::GetVersion() == base::win::Version::WIN7) {
    std::move(callback).Run(Result::kOk);
    return;
  }
#endif  // BUILDFLAG(IS_WIN)

  if (!protocol_handler_manager_) {
    std::move(callback).Run(Result::kOk);
    return;
  }

  protocol_handler_manager_->UnregisterOsProtocolHandlers(app_id,
                                                          std::move(callback));
}

void OsIntegrationManager::UnregisterUrlHandlers(const AppId& app_id) {
  if (!url_handler_manager_)
    return;

  url_handler_manager_->UnregisterUrlHandlers(app_id);
}

void OsIntegrationManager::UnregisterWebAppOsUninstallation(
    const AppId& app_id) {
  if (ShouldRegisterUninstallationViaOsSettingsWithOs())
    UnegisterUninstallationViaOsSettingsWithOs(app_id, profile_);
}

void OsIntegrationManager::UpdateShortcuts(const AppId& app_id,
                                           base::StringPiece old_name,
                                           base::OnceClosure callback) {
  DCHECK(shortcut_manager_);
  shortcut_manager_->UpdateShortcuts(app_id, old_name, std::move(callback));
}

void OsIntegrationManager::UpdateShortcutsMenu(
    const AppId& app_id,
    const WebAppInstallInfo& web_app_info) {
  DCHECK(shortcut_manager_);
  if (web_app_info.shortcuts_menu_item_infos.empty()) {
    shortcut_manager_->UnregisterShortcutsMenuWithOs(app_id);
  } else {
    shortcut_manager_->RegisterShortcutsMenuWithOs(
        app_id, web_app_info.shortcuts_menu_item_infos,
        web_app_info.shortcuts_menu_icon_bitmaps);
  }
}

void OsIntegrationManager::UpdateUrlHandlers(
    const AppId& app_id,
    base::OnceCallback<void(bool success)> callback) {
  if (!url_handler_manager_)
    return;

  url_handler_manager_->UpdateUrlHandlers(app_id, std::move(callback));
}

void OsIntegrationManager::UpdateFileHandlers(
    const AppId& app_id,
    FileHandlerUpdateAction file_handlers_need_os_update,
    ResultCallback finished_callback) {
  if (file_handlers_need_os_update == FileHandlerUpdateAction::kNoUpdate) {
    std::move(finished_callback).Run(Result::kOk);
    return;
  }

  if (file_handlers_need_os_update == FileHandlerUpdateAction::kUpdate &&
      !IsFileHandlingAPIAvailable(app_id)) {
    std::move(finished_callback).Run(Result::kOk);
    return;
  }

  ResultCallback callback_after_removal;
  if (file_handlers_need_os_update == FileHandlerUpdateAction::kUpdate) {
    callback_after_removal = base::BindOnce(
        [](base::WeakPtr<OsIntegrationManager> os_integration_manager,
           const AppId& app_id, ResultCallback finished_callback,
           Result result) {
          // Re-register file handlers regardless of `result`.
          // TODO(https://crbug.com/1124047): Report `result` in
          // an UMA metric.
          if (!os_integration_manager) {
            std::move(finished_callback).Run(Result::kError);
            return;
          }
          os_integration_manager->RegisterFileHandlers(
              app_id, std::move(finished_callback));
        },
        weak_ptr_factory_.GetWeakPtr(), app_id, std::move(finished_callback));
  } else {
    DCHECK_EQ(file_handlers_need_os_update, FileHandlerUpdateAction::kRemove);
    callback_after_removal = std::move(finished_callback);
  }

  // Update file handlers via complete uninstallation, then potential
  // reinstallation.
  UnregisterFileHandlers(app_id, std::move(callback_after_removal));
}

void OsIntegrationManager::UpdateProtocolHandlers(
    const AppId& app_id,
    bool force_shortcut_updates_if_needed,
    base::OnceClosure callback) {
  if (!protocol_handler_manager_) {
    std::move(callback).Run();
    return;
  }

  // Disable protocol handler unregistration on Win7 due to bad interactions
  // between preinstalled app scenarios and the need for elevation to unregister
  // protocol handlers on that platform. See crbug.com/1224327 for context.
#if BUILDFLAG(IS_WIN)
  if (base::win::GetVersion() == base::win::Version::WIN7) {
    std::move(callback).Run();
    return;
  }
#endif  // BUILDFLAG(IS_WIN)

  auto shortcuts_callback = base::BindOnce(
      &OsIntegrationManager::OnShortcutsUpdatedForProtocolHandlers,
      weak_ptr_factory_.GetWeakPtr(), app_id, std::move(callback));

#if !BUILDFLAG(IS_WIN)
  // Windows handles protocol registration through the registry. For other
  // OS's we also need to regenerate the shortcut file before we call into
  // the OS. Since `UpdateProtocolHandlers` function is also called in
  // `UpdateOSHooks`, which also recreates the shortcuts, only do it if
  // required.
  if (force_shortcut_updates_if_needed) {
    UpdateShortcuts(app_id, "", std::move(shortcuts_callback));
    return;
  }
#endif

  std::move(shortcuts_callback).Run();
}

void OsIntegrationManager::OnShortcutsUpdatedForProtocolHandlers(
    const AppId& app_id,
    base::OnceClosure update_finished_callback) {
  // Update protocol handlers via complete uninstallation, then reinstallation.
  ResultCallback unregister_callback = base::BindOnce(
      [](base::WeakPtr<OsIntegrationManager> os_integration_manager,
         const AppId& app_id, base::OnceClosure update_finished_callback,
         Result result) {
        // Re-register protocol handlers regardless of `result`.
        // TODO(https://crbug.com/1250728): Report a UMA metric when
        // unregistering fails, either here, or at the point of failure. This
        // might also mean we can remove `result`.
        if (!os_integration_manager) {
          std::move(update_finished_callback).Run();
          return;
        }

        os_integration_manager->RegisterProtocolHandlers(
            app_id,
            base::BindOnce(
                [](base::OnceClosure update_finished_callback, Result result) {
                  // TODO(https://crbug.com/1250728): Report
                  // |result| in an UMA metric.
                  std::move(update_finished_callback).Run();
                },
                std::move(update_finished_callback)));
      },
      weak_ptr_factory_.GetWeakPtr(), app_id,
      std::move(update_finished_callback));

  UnregisterProtocolHandlers(app_id, std::move(unregister_callback));
}

void OsIntegrationManager::OnWebAppProfileWillBeDeleted(const AppId& app_id) {
  UninstallAllOsHooks(app_id, base::DoNothing());
}

std::unique_ptr<ShortcutInfo> OsIntegrationManager::BuildShortcutInfo(
    const AppId& app_id) {
  DCHECK(shortcut_manager_);
  return shortcut_manager_->BuildShortcutInfo(app_id);
}

void OsIntegrationManager::OnShortcutsCreated(
    const AppId& app_id,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    InstallOsHooksOptions options,
    scoped_refptr<OsHooksBarrier> barrier,
    bool shortcuts_created) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(barrier);

  bool shortcut_creation_failure =
      !shortcuts_created && options.os_hooks[OsHookType::kShortcuts];
  if (shortcut_creation_failure)
    barrier->OnError(OsHookType::kShortcuts);

#if !BUILDFLAG(IS_MAC)
  // This step happens before shortcut creation on Mac.
  if (options.os_hooks[OsHookType::kFileHandlers]) {
    RegisterFileHandlers(app_id, barrier->CreateBarrierCallbackForType(
                                     OsHookType::kFileHandlers));
  }
#endif

  if (options.os_hooks[OsHookType::kProtocolHandlers]) {
    RegisterProtocolHandlers(app_id, barrier->CreateBarrierCallbackForType(
                                         OsHookType::kProtocolHandlers));
  }

  if (options.os_hooks[OsHookType::kUrlHandlers]) {
    RegisterUrlHandlers(app_id, barrier->CreateBarrierCallbackForType(
                                    OsHookType::kUrlHandlers));
  }

  if (options.os_hooks[OsHookType::kShortcuts] &&
      options.add_to_quick_launch_bar) {
    AddAppToQuickLaunchBar(app_id);
  }
  if (shortcuts_created && options.os_hooks[OsHookType::kShortcutsMenu]) {
    if (web_app_info) {
      RegisterShortcutsMenu(
          app_id, web_app_info->shortcuts_menu_item_infos,
          web_app_info->shortcuts_menu_icon_bitmaps,
          barrier->CreateBarrierCallbackForType(OsHookType::kShortcutsMenu));
    } else {
      ReadAllShortcutsMenuIconsAndRegisterShortcutsMenu(
          app_id,
          barrier->CreateBarrierCallbackForType(OsHookType::kShortcutsMenu));
    }
  }

  if (options.os_hooks[OsHookType::kRunOnOsLogin] &&
      base::FeatureList::IsEnabled(features::kDesktopPWAsRunOnOsLogin)) {
    // TODO(crbug.com/1091964): Implement Run on OS Login mode selection.
    // Currently it is set to be the default: RunOnOsLoginMode::kWindowed
    RegisterRunOnOsLogin(app_id, barrier->CreateBarrierCallbackForType(
                                     OsHookType::kRunOnOsLogin));
  }

  if (options.os_hooks[OsHookType::kUninstallationViaOsSettings] &&
      base::FeatureList::IsEnabled(
          features::kEnableWebAppUninstallFromOsSettings)) {
    RegisterWebAppOsUninstallation(
        app_id, registrar_ ? registrar_->GetAppShortName(app_id) : "");
  }
}

void OsIntegrationManager::OnShortcutsDeleted(const AppId& app_id,
                                              ResultCallback callback,
                                              Result result) {
#if BUILDFLAG(IS_MAC)
  bool delete_multi_profile_shortcuts =
      AppShimRegistry::Get()->OnAppUninstalledForProfile(app_id,
                                                         profile_->GetPath());
  if (delete_multi_profile_shortcuts) {
    internals::ScheduleDeleteMultiProfileShortcutsForApp(app_id,
                                                         std::move(callback));
  }
#else
  std::move(callback).Run(result);
#endif
}

void OsIntegrationManager::OnShortcutInfoRetrievedRegisterRunOnOsLogin(
    RegisterRunOnOsLoginCallback callback,
    std::unique_ptr<ShortcutInfo> info) {
  ScheduleRegisterRunOnOsLogin(std::move(info), std::move(callback));
}

}  // namespace web_app
