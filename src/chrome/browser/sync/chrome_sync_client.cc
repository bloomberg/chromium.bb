// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/chrome_sync_client.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/path_service.h"
#include "base/syslog_logging.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/password_manager/account_storage/account_password_store_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/security_events/security_event_recorder.h"
#include "chrome/browser/security_events/security_event_recorder_factory.h"
#include "chrome/browser/sharing/sharing_message_bridge.h"
#include "chrome/browser/sharing/sharing_message_bridge_factory.h"
#include "chrome/browser/sharing/sharing_message_model_type_controller.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/bookmark_sync_service_factory.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/themes/theme_syncable_service.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/core/browser/webdata/autocomplete_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_profile_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_wallet_metadata_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_wallet_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/browser_sync/profile_sync_components_factory_impl.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/common/pref_names.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/search_engines/template_url_service.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/base/sync_base_switches.h"
#include "components/sync/driver/file_based_trusted_vault_client.h"
#include "components/sync/driver/model_type_controller.h"
#include "components/sync/driver/sync_api_component_factory.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/driver/sync_util.h"
#include "components/sync/driver/syncable_service_based_model_type_controller.h"
#include "components/sync/engine/passive_model_worker.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_store_service.h"
#include "components/sync/model_impl/forwarding_model_type_controller_delegate.h"
#include "components/sync_bookmarks/bookmark_sync_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_user_events/user_event_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/storage/backend_task_runner.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/storage/settings_sync_util.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/browser/sync/glue/extension_model_type_controller.h"
#include "chrome/browser/sync/glue/extension_setting_model_type_controller.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_sync_model_type_controller.h"
#include "chrome/browser/supervised_user/supervised_user_whitelist_service.h"
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

#if BUILDFLAG(ENABLE_SPELLCHECK)
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

#if defined(OS_ANDROID)
#include "chrome/browser/sync/trusted_vault_client_android.h"
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/printing/printers_sync_bridge.h"
#include "chrome/browser/chromeos/printing/synced_printers_manager.h"
#include "chrome/browser/chromeos/printing/synced_printers_manager_factory.h"
#include "chrome/browser/chromeos/sync/app_settings_model_type_controller.h"
#include "chrome/browser/chromeos/sync/apps_model_type_controller.h"
#include "chrome/browser/chromeos/sync/os_sync_model_type_controller.h"
#include "chrome/browser/chromeos/sync/os_syncable_service_model_type_controller.h"
#include "chrome/browser/sync/wifi_configuration_sync_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_package_sync_model_type_controller.h"
#include "chrome/browser/ui/app_list/arc/arc_package_syncable_service.h"
#include "chromeos/components/sync_wifi/wifi_configuration_sync_service.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/arc/arc_util.h"
#endif  // defined(OS_CHROMEOS)

using content::BrowserThread;
using syncer::ForwardingModelTypeControllerDelegate;

#if BUILDFLAG(ENABLE_EXTENSIONS)
using browser_sync::ExtensionModelTypeController;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace browser_sync {

namespace {

#if !defined(OS_ANDROID)
const base::FilePath::CharType kTrustedVaultFilename[] =
    FILE_PATH_LITERAL("Trusted Vault");
#endif  // !defined(OS_ANDROID)

#if defined(OS_WIN)
const base::FilePath::CharType kLoopbackServerBackendFilename[] =
    FILE_PATH_LITERAL("profile.pb");
#endif  // defined(OS_WIN)

syncer::ModelTypeSet GetDisabledTypesFromCommandLine() {
  std::string disabled_types_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kDisableSyncTypes);

  syncer::ModelTypeSet disabled_types =
      syncer::ModelTypeSetFromString(disabled_types_str);
  if (disabled_types.Has(syncer::DEVICE_INFO)) {
    DLOG(WARNING) << "DEVICE_INFO cannot be disabled via a command-line switch";
    disabled_types.Remove(syncer::DEVICE_INFO);
  }
  return disabled_types;
}

base::WeakPtr<syncer::SyncableService> GetWeakPtrOrNull(
    syncer::SyncableService* service) {
  return service ? service->AsWeakPtr() : nullptr;
}

base::RepeatingClosure GetDumpStackClosure() {
  return base::BindRepeating(&syncer::ReportUnrecoverableError,
                             chrome::GetChannel());
}

}  // namespace

ChromeSyncClient::ChromeSyncClient(Profile* profile) : profile_(profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  profile_web_data_service_ =
      WebDataServiceFactory::GetAutofillWebDataForProfile(
          profile_, ServiceAccessType::IMPLICIT_ACCESS);
  account_web_data_service_ =
      base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableAccountWalletStorage)
          ? WebDataServiceFactory::GetAutofillWebDataForAccount(
                profile_, ServiceAccessType::IMPLICIT_ACCESS)
          : nullptr;
  web_data_service_thread_ = profile_web_data_service_
                                 ? profile_web_data_service_->GetDBTaskRunner()
                                 : nullptr;

  // This class assumes that the database thread is the same across the profile
  // and account storage. This DCHECK makes that assumption explicit.
  DCHECK(!account_web_data_service_ ||
         web_data_service_thread_ ==
             account_web_data_service_->GetDBTaskRunner());
  profile_password_store_ = PasswordStoreFactory::GetForProfile(
      profile_, ServiceAccessType::IMPLICIT_ACCESS);
  account_password_store_ = AccountPasswordStoreFactory::GetForProfile(
      profile_, ServiceAccessType::IMPLICIT_ACCESS);

  component_factory_ = std::make_unique<ProfileSyncComponentsFactoryImpl>(
      this, chrome::GetChannel(), prefs::kSavingBrowserHistoryDisabled,
      base::CreateSequencedTaskRunner({content::BrowserThread::UI}),
      web_data_service_thread_, profile_web_data_service_,
      account_web_data_service_, profile_password_store_,
      account_password_store_,
      BookmarkSyncServiceFactory::GetForProfile(profile_));

#if defined(OS_ANDROID)
  trusted_vault_client_ = std::make_unique<TrustedVaultClientAndroid>();
#else
  trusted_vault_client_ = std::make_unique<syncer::FileBasedTrustedVaultClient>(
      profile_->GetPath().Append(kTrustedVaultFilename));
#endif  // defined(OS_ANDROID)
}

ChromeSyncClient::~ChromeSyncClient() {}

PrefService* ChromeSyncClient::GetPrefService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return profile_->GetPrefs();
}

signin::IdentityManager* ChromeSyncClient::GetIdentityManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return IdentityManagerFactory::GetForProfile(profile_);
}

base::FilePath ChromeSyncClient::GetLocalSyncBackendFolder() {
  base::FilePath local_sync_backend_folder =
      GetPrefService()->GetFilePath(syncer::prefs::kLocalSyncBackendDir);

#if defined(OS_WIN)
  if (local_sync_backend_folder.empty()) {
    if (!base::PathService::Get(chrome::DIR_ROAMING_USER_DATA,
                                &local_sync_backend_folder)) {
      SYSLOG(WARNING) << "Local sync can not get the roaming profile folder.";
      return base::FilePath();
    }
  }

  // This code as it is now will assume the same profile order is present on
  // all machines, which is not a given. It is to be defined if only the
  // Default profile should get this treatment or all profile as is the case
  // now.
  // TODO(pastarmovj): http://crbug.com/674928 Decide if only the Default one
  // should be considered roamed. For now the code assumes all profiles are
  // created in the same order on all machines.
  local_sync_backend_folder =
      local_sync_backend_folder.Append(profile_->GetPath().BaseName());
  local_sync_backend_folder =
      local_sync_backend_folder.Append(kLoopbackServerBackendFilename);
#endif  // defined(OS_WIN)

  return local_sync_backend_folder;
}

syncer::ModelTypeStoreService* ChromeSyncClient::GetModelTypeStoreService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return ModelTypeStoreServiceFactory::GetForProfile(profile_);
}

syncer::DeviceInfoSyncService* ChromeSyncClient::GetDeviceInfoSyncService() {
  return DeviceInfoSyncServiceFactory::GetForProfile(profile_);
}

bookmarks::BookmarkModel* ChromeSyncClient::GetBookmarkModel() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return BookmarkModelFactory::GetForBrowserContext(profile_);
}

favicon::FaviconService* ChromeSyncClient::GetFaviconService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return FaviconServiceFactory::GetForProfile(
      profile_, ServiceAccessType::IMPLICIT_ACCESS);
}

history::HistoryService* ChromeSyncClient::GetHistoryService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
}

send_tab_to_self::SendTabToSelfSyncService*
ChromeSyncClient::GetSendTabToSelfSyncService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return SendTabToSelfSyncServiceFactory::GetForProfile(profile_);
}

sync_sessions::SessionSyncService* ChromeSyncClient::GetSessionSyncService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return SessionSyncServiceFactory::GetForProfile(profile_);
}

base::Closure ChromeSyncClient::GetPasswordStateChangedCallback() {
  return base::Bind(
      &PasswordStoreFactory::OnPasswordsSyncedStatePotentiallyChanged,
      base::Unretained(profile_));
}

syncer::DataTypeController::TypeVector
ChromeSyncClient::CreateDataTypeControllers(syncer::SyncService* sync_service) {
  syncer::ModelTypeSet disabled_types = GetDisabledTypesFromCommandLine();

  syncer::DataTypeController::TypeVector controllers =
      component_factory_->CreateCommonDataTypeControllers(disabled_types,
                                                          sync_service);

  const base::RepeatingClosure dump_stack = GetDumpStackClosure();

  syncer::RepeatingModelTypeStoreFactory model_type_store_factory =
      GetModelTypeStoreService()->GetStoreFactory();

  if (!disabled_types.Has(syncer::SECURITY_EVENTS)) {
    syncer::ModelTypeControllerDelegate* delegate =
        SecurityEventRecorderFactory::GetForProfile(profile_)
            ->GetControllerDelegate()
            .get();
    // Forward both full-sync and transport-only modes to the same delegate,
    // since behavior for SECURITY_EVENTS does not differ.
    controllers.push_back(std::make_unique<syncer::ModelTypeController>(
        syncer::SECURITY_EVENTS,
        /*delegate_for_full_sync_mode=*/
        std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
            delegate),
        /*delegate_for_transport_mode=*/
        std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
            delegate)));
  }

  if (!disabled_types.Has(syncer::SHARING_MESSAGE)) {
    // Forward both full-sync and transport-only modes to the same delegate,
    // since behavior for SHARING_MESSAGE does not differ. They both do not
    // store data on persistent storage.
    syncer::ModelTypeControllerDelegate* delegate =
        GetControllerDelegateForModelType(syncer::SHARING_MESSAGE).get();
    controllers.push_back(std::make_unique<SharingMessageModelTypeController>(
        sync_service,
        /*delegate_for_full_sync_mode=*/
        std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
            delegate),
        /*delegate_for_transport_mode=*/
        std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
            delegate)));
  }

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  controllers.push_back(std::make_unique<SupervisedUserSyncModelTypeController>(
      syncer::SUPERVISED_USER_SETTINGS, profile_, dump_stack,
      model_type_store_factory,
      GetSyncableServiceForType(syncer::SUPERVISED_USER_SETTINGS)));
  controllers.push_back(std::make_unique<SupervisedUserSyncModelTypeController>(
      syncer::SUPERVISED_USER_WHITELISTS, profile_, dump_stack,
      model_type_store_factory,
      GetSyncableServiceForType(syncer::SUPERVISED_USER_WHITELISTS)));
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // App sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::APPS)) {
    controllers.push_back(CreateAppsModelTypeController(sync_service));
  }

  // Extension sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::EXTENSIONS)) {
    controllers.push_back(std::make_unique<ExtensionModelTypeController>(
        syncer::EXTENSIONS, model_type_store_factory,
        GetSyncableServiceForType(syncer::EXTENSIONS), dump_stack, profile_));
  }

  // Extension setting sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::EXTENSION_SETTINGS)) {
    controllers.push_back(std::make_unique<ExtensionSettingModelTypeController>(
        syncer::EXTENSION_SETTINGS, model_type_store_factory,
        extensions::settings_sync_util::GetSyncableServiceProvider(
            profile_, syncer::EXTENSION_SETTINGS),
        dump_stack, profile_));
  }

  // App setting sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::APP_SETTINGS)) {
    controllers.push_back(CreateAppSettingsModelTypeController(sync_service));
  }

  // Web Apps sync is disabled by default.
  if (base::FeatureList::IsEnabled(features::kDesktopPWAsWithoutExtensions) &&
      web_app::WebAppProvider::Get(profile_)) {
    if (!disabled_types.Has(syncer::WEB_APPS)) {
      controllers.push_back(CreateWebAppsModelTypeController(sync_service));
    }
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if !defined(OS_ANDROID)
  // Theme sync is enabled by default.  Register unless explicitly disabled.
  if (!disabled_types.Has(syncer::THEMES)) {
    controllers.push_back(std::make_unique<ExtensionModelTypeController>(
        syncer::THEMES, model_type_store_factory,
        GetSyncableServiceForType(syncer::THEMES), dump_stack, profile_));
  }

  // Search Engine sync is enabled by default.  Register unless explicitly
  // disabled. The service can be null in tests.
  if (!disabled_types.Has(syncer::SEARCH_ENGINES)) {
    controllers.push_back(
        std::make_unique<syncer::SyncableServiceBasedModelTypeController>(
            syncer::SEARCH_ENGINES, model_type_store_factory,
            GetSyncableServiceForType(syncer::SEARCH_ENGINES), dump_stack));
  }
#endif  // !defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
  // Some profile types (e.g. sign-in screen) don't support app list.
  // Temporarily Disable AppListSyncableService for tablet form factor devices.
  // See crbug/1013732 for details.
  if (app_list::AppListSyncableServiceFactory::GetForProfile(profile_) &&
      !chromeos::switches::IsTabletFormFactor()) {
    if (chromeos::features::IsSplitSettingsSyncEnabled()) {
      // Runs in sync transport-mode and full-sync mode.
      controllers.push_back(
          std::make_unique<OsSyncableServiceModelTypeController>(
              syncer::APP_LIST, model_type_store_factory,
              GetSyncableServiceForType(syncer::APP_LIST), dump_stack,
              profile_->GetPrefs(), sync_service));
    } else {
      // Only runs in full-sync mode.
      controllers.push_back(
          std::make_unique<syncer::SyncableServiceBasedModelTypeController>(
              syncer::APP_LIST, model_type_store_factory,
              GetSyncableServiceForType(syncer::APP_LIST), dump_stack));
    }
  }
#endif  // defined(OS_CHROMEOS)

// Chrome prefers OS provided spell checkers where they exist. So only sync the
// custom dictionary on platforms that typically don't provide one.
#if defined(OS_LINUX) || defined(OS_WIN)
  // Dictionary sync is enabled by default.
  if (!disabled_types.Has(syncer::DICTIONARY)) {
    controllers.push_back(
        std::make_unique<syncer::SyncableServiceBasedModelTypeController>(
            syncer::DICTIONARY, model_type_store_factory,
            GetSyncableServiceForType(syncer::DICTIONARY), dump_stack));
  }
#endif  // defined(OS_LINUX) || defined(OS_WIN)

#if defined(OS_CHROMEOS)
  if (arc::IsArcAllowedForProfile(profile_) &&
      !arc::IsArcAppSyncFlowDisabled()) {
    controllers.push_back(std::make_unique<ArcPackageSyncModelTypeController>(
        model_type_store_factory,
        GetSyncableServiceForType(syncer::ARC_PACKAGE), dump_stack,
        sync_service, profile_));
  }
  if (chromeos::features::IsSplitSettingsSyncEnabled()) {
    if (!disabled_types.Has(syncer::OS_PREFERENCES)) {
      controllers.push_back(
          std::make_unique<OsSyncableServiceModelTypeController>(
              syncer::OS_PREFERENCES, model_type_store_factory,
              GetSyncableServiceForType(syncer::OS_PREFERENCES), dump_stack,
              profile_->GetPrefs(), sync_service));
    }
    if (!disabled_types.Has(syncer::OS_PRIORITY_PREFERENCES)) {
      controllers.push_back(
          std::make_unique<OsSyncableServiceModelTypeController>(
              syncer::OS_PRIORITY_PREFERENCES, model_type_store_factory,
              GetSyncableServiceForType(syncer::OS_PRIORITY_PREFERENCES),
              dump_stack, profile_->GetPrefs(), sync_service));
    }
    if (!disabled_types.Has(syncer::PRINTERS)) {
      // Use the same delegate in full-sync and transport-only modes.
      syncer::ModelTypeControllerDelegate* delegate =
          GetControllerDelegateForModelType(syncer::PRINTERS).get();
      controllers.push_back(std::make_unique<OsSyncModelTypeController>(
          syncer::PRINTERS,
          /*delegate_for_full_sync_mode=*/
          std::make_unique<ForwardingModelTypeControllerDelegate>(delegate),
          /*delegate_for_transport_mode=*/
          std::make_unique<ForwardingModelTypeControllerDelegate>(delegate),
          profile_->GetPrefs(), sync_service));
    }
    if (!disabled_types.Has(syncer::WIFI_CONFIGURATIONS) &&
        base::FeatureList::IsEnabled(switches::kSyncWifiConfigurations) &&
        WifiConfigurationSyncServiceFactory::ShouldRunInProfile(profile_)) {
      // Use the same delegate in full-sync and transport-only modes.
      syncer::ModelTypeControllerDelegate* delegate =
          GetControllerDelegateForModelType(syncer::WIFI_CONFIGURATIONS).get();
      controllers.push_back(std::make_unique<OsSyncModelTypeController>(
          syncer::WIFI_CONFIGURATIONS,
          /*delegate_for_full_sync_mode=*/
          std::make_unique<ForwardingModelTypeControllerDelegate>(delegate),
          /*delegate_for_transport_mode=*/
          std::make_unique<ForwardingModelTypeControllerDelegate>(delegate),
          profile_->GetPrefs(), sync_service));
    }
  } else {
    // SplitSettingsSync is disabled.
    if (!disabled_types.Has(syncer::WIFI_CONFIGURATIONS) &&
        base::FeatureList::IsEnabled(switches::kSyncWifiConfigurations) &&
        WifiConfigurationSyncServiceFactory::ShouldRunInProfile(profile_)) {
      syncer::ModelTypeControllerDelegate* delegate =
          GetControllerDelegateForModelType(syncer::WIFI_CONFIGURATIONS).get();
      controllers.push_back(std::make_unique<syncer::ModelTypeController>(
          syncer::WIFI_CONFIGURATIONS,
          std::make_unique<ForwardingModelTypeControllerDelegate>(delegate)));
    }
  }
#endif  // defined(OS_CHROMEOS)

  return controllers;
}

BookmarkUndoService* ChromeSyncClient::GetBookmarkUndoService() {
  return BookmarkUndoServiceFactory::GetForProfile(profile_);
}

syncer::TrustedVaultClient* ChromeSyncClient::GetTrustedVaultClient() {
  return trusted_vault_client_.get();
}

invalidation::InvalidationService* ChromeSyncClient::GetInvalidationService() {
  invalidation::ProfileInvalidationProvider* provider =
      invalidation::ProfileInvalidationProviderFactory::GetForProfile(profile_);

  if (provider)
    return provider->GetInvalidationService();
  return nullptr;
}

scoped_refptr<syncer::ExtensionsActivity>
ChromeSyncClient::GetExtensionsActivity() {
  return extensions_activity_monitor_.GetExtensionsActivity();
}

base::WeakPtr<syncer::SyncableService>
ChromeSyncClient::GetSyncableServiceForType(syncer::ModelType type) {
  switch (type) {
    case syncer::PREFERENCES:
      return PrefServiceSyncableFromProfile(profile_)
          ->GetSyncableService(syncer::PREFERENCES)
          ->AsWeakPtr();
    case syncer::PRIORITY_PREFERENCES:
      return PrefServiceSyncableFromProfile(profile_)
          ->GetSyncableService(syncer::PRIORITY_PREFERENCES)
          ->AsWeakPtr();
    case syncer::SEARCH_ENGINES:
      return GetWeakPtrOrNull(
          TemplateURLServiceFactory::GetForProfile(profile_));
#if BUILDFLAG(ENABLE_EXTENSIONS)
    case syncer::APPS:
    case syncer::EXTENSIONS:
      return GetWeakPtrOrNull(ExtensionSyncService::Get(profile_));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
#if defined(OS_CHROMEOS)
    case syncer::APP_LIST:
      return GetWeakPtrOrNull(
          app_list::AppListSyncableServiceFactory::GetForProfile(profile_));
#endif  // defined(OS_CHROMEOS)
#if !defined(OS_ANDROID)
    case syncer::THEMES:
      return ThemeServiceFactory::GetForProfile(profile_)->
          GetThemeSyncableService()->AsWeakPtr();
#endif  // !defined(OS_ANDROID)
    case syncer::HISTORY_DELETE_DIRECTIVES: {
      history::HistoryService* history = GetHistoryService();
      return history ? history->GetDeleteDirectivesSyncableService() : nullptr;
    }
#if BUILDFLAG(ENABLE_SPELLCHECK)
    case syncer::DICTIONARY: {
      SpellcheckService* spellcheck_service =
          SpellcheckServiceFactory::GetForContext(profile_);
      return spellcheck_service
                 ? spellcheck_service->GetCustomDictionary()->AsWeakPtr()
                 : nullptr;
    }
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
    case syncer::SUPERVISED_USER_SETTINGS:
      return SupervisedUserSettingsServiceFactory::GetForKey(
                 profile_->GetProfileKey())
          ->AsWeakPtr();
    case syncer::SUPERVISED_USER_WHITELISTS:
      return SupervisedUserServiceFactory::GetForProfile(profile_)
          ->GetWhitelistService()
          ->AsWeakPtr();
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)
#if defined(OS_CHROMEOS)
    case syncer::ARC_PACKAGE:
      return arc::ArcPackageSyncableService::Get(profile_)->AsWeakPtr();
    case syncer::OS_PREFERENCES:
    case syncer::OS_PRIORITY_PREFERENCES:
      return PrefServiceSyncableFromProfile(profile_)
          ->GetSyncableService(type)
          ->AsWeakPtr();
#endif  // defined(OS_CHROMEOS)
    default:
      NOTREACHED();
      return nullptr;
  }
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
ChromeSyncClient::GetControllerDelegateForModelType(syncer::ModelType type) {
  switch (type) {
    case syncer::READING_LIST:
      // Reading List is only supported on iOS at the moment.
      NOTREACHED();
      return base::WeakPtr<syncer::ModelTypeControllerDelegate>();
#if defined(OS_CHROMEOS)
    case syncer::PRINTERS:
      return chromeos::SyncedPrintersManagerFactory::GetForBrowserContext(
                 profile_)
          ->GetSyncBridge()
          ->change_processor()
          ->GetControllerDelegate();
    case syncer::WIFI_CONFIGURATIONS:
      return WifiConfigurationSyncServiceFactory::GetForProfile(profile_,
                                                                /*create=*/true)
          ->GetControllerDelegate();
#endif  // defined(OS_CHROMEOS)
    case syncer::SHARING_MESSAGE:
      return SharingMessageBridgeFactory::GetForBrowserContext(profile_)
          ->GetControllerDelegate();
    case syncer::USER_CONSENTS:
      return ConsentAuditorFactory::GetForProfile(profile_)
          ->GetControllerDelegate();
    case syncer::USER_EVENTS:
      return browser_sync::UserEventServiceFactory::GetForProfile(profile_)
          ->GetSyncBridge()
          ->change_processor()
          ->GetControllerDelegate();
#if BUILDFLAG(ENABLE_EXTENSIONS)
    case syncer::WEB_APPS: {
      DCHECK(base::FeatureList::IsEnabled(
          features::kDesktopPWAsWithoutExtensions));
      auto* provider = web_app::WebAppProvider::Get(profile_);
      DCHECK(provider);

      web_app::WebAppSyncBridge* sync_bridge =
          provider->registry_controller().AsWebAppSyncBridge();
      DCHECK(sync_bridge);

      return sync_bridge->change_processor()->GetControllerDelegate();
    }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
    // We don't exercise this function for certain datatypes, because their
    // controllers get the delegate elsewhere.
    case syncer::AUTOFILL:
    case syncer::AUTOFILL_PROFILE:
    case syncer::AUTOFILL_WALLET_DATA:
    case syncer::AUTOFILL_WALLET_METADATA:
    case syncer::BOOKMARKS:
    case syncer::DEVICE_INFO:
    case syncer::SECURITY_EVENTS:
    case syncer::SEND_TAB_TO_SELF:
    case syncer::SESSIONS:
    case syncer::TYPED_URLS:
      NOTREACHED();
      return base::WeakPtr<syncer::ModelTypeControllerDelegate>();

    default:
      NOTREACHED();
      return base::WeakPtr<syncer::ModelTypeControllerDelegate>();
  }
}

scoped_refptr<syncer::ModelSafeWorker>
ChromeSyncClient::CreateModelWorkerForGroup(syncer::ModelSafeGroup group) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  switch (group) {
    case syncer::GROUP_PASSIVE:
      return new syncer::PassiveModelWorker();
    default:
      return nullptr;
  }
}

syncer::SyncApiComponentFactory*
ChromeSyncClient::GetSyncApiComponentFactory() {
  return component_factory_.get();
}

syncer::SyncTypePreferenceProvider* ChromeSyncClient::GetPreferenceProvider() {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  return SupervisedUserServiceFactory::GetForProfile(profile_);
#else
  return nullptr;
#endif
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
std::unique_ptr<syncer::ModelTypeController>
ChromeSyncClient::CreateAppsModelTypeController(
    syncer::SyncService* sync_service) {
#if defined(OS_CHROMEOS)
  if (chromeos::features::IsSplitSettingsSyncEnabled()) {
    return AppsModelTypeController::Create(
        GetModelTypeStoreService()->GetStoreFactory(),
        GetSyncableServiceForType(syncer::APPS), GetDumpStackClosure(),
        sync_service, profile_);
  }
  // Fall through.
#endif
  return std::make_unique<ExtensionModelTypeController>(
      syncer::APPS, GetModelTypeStoreService()->GetStoreFactory(),
      GetSyncableServiceForType(syncer::APPS), GetDumpStackClosure(), profile_);
}

std::unique_ptr<syncer::ModelTypeController>
ChromeSyncClient::CreateAppSettingsModelTypeController(
    syncer::SyncService* sync_service) {
#if defined(OS_CHROMEOS)
  if (chromeos::features::IsSplitSettingsSyncEnabled()) {
    return std::make_unique<AppSettingsModelTypeController>(
        GetModelTypeStoreService()->GetStoreFactory(),
        extensions::settings_sync_util::GetSyncableServiceProvider(
            profile_, syncer::APP_SETTINGS),
        GetDumpStackClosure(), profile_, sync_service);
  }
  // Fall through.
#endif
  return std::make_unique<ExtensionSettingModelTypeController>(
      syncer::APP_SETTINGS, GetModelTypeStoreService()->GetStoreFactory(),
      extensions::settings_sync_util::GetSyncableServiceProvider(
          profile_, syncer::APP_SETTINGS),
      GetDumpStackClosure(), profile_);
}

std::unique_ptr<syncer::ModelTypeController>
ChromeSyncClient::CreateWebAppsModelTypeController(
    syncer::SyncService* sync_service) {
  syncer::ModelTypeControllerDelegate* delegate =
      GetControllerDelegateForModelType(syncer::WEB_APPS).get();
#if defined(OS_CHROMEOS)
  if (chromeos::features::IsSplitSettingsSyncEnabled()) {
    // Use the same delegate in full-sync and transport-only modes.
    return std::make_unique<OsSyncModelTypeController>(
        syncer::WEB_APPS,
        /*delegate_for_full_sync_mode=*/
        std::make_unique<ForwardingModelTypeControllerDelegate>(delegate),
        /*delegate_for_transport_mode=*/
        std::make_unique<ForwardingModelTypeControllerDelegate>(delegate),
        profile_->GetPrefs(), sync_service);
  }
  // Fall through.
#endif
  return std::make_unique<syncer::ModelTypeController>(
      syncer::WEB_APPS,
      std::make_unique<ForwardingModelTypeControllerDelegate>(delegate));
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace browser_sync
