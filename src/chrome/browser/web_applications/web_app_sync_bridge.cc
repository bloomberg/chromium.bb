// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_sync_bridge.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/types/pass_key.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_database.h"
#include "chrome/browser/web_applications/web_app_database_factory.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/web_app_proto_utils.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_sync_install_delegate.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/channel_info.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "content/public/common/content_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/feature_list.h"
#include "chrome/common/chrome_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace web_app {

std::unique_ptr<syncer::EntityData> CreateSyncEntityData(const WebApp& app) {
  // The Sync System doesn't allow empty entity_data name.
  DCHECK(!app.name().empty());

  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->name = app.name();
  // TODO(crbug.com/1103570): Remove this fallback later.
  if (entity_data->name.empty())
    entity_data->name = app.start_url().spec();

  *(entity_data->specifics.mutable_web_app()) = WebAppToSyncProto(app);
  return entity_data;
}

void ApplySyncDataToApp(const sync_pb::WebAppSpecifics& sync_data,
                        WebApp* app) {
  app->AddSource(Source::kSync);

  // app_id is a hash of start_url. Parse start_url first:
  GURL start_url(sync_data.start_url());
  if (start_url.is_empty() || !start_url.is_valid()) {
    DLOG(ERROR) << "ApplySyncDataToApp: start_url parse error.";
    return;
  }
  absl::optional<std::string> manifest_id = absl::nullopt;
  if (sync_data.has_manifest_id())
    manifest_id = absl::optional<std::string>(sync_data.manifest_id());

  if (app->app_id() != GenerateAppId(manifest_id, start_url)) {
    DLOG(ERROR) << "ApplySyncDataToApp: app_id doesn't match id generated "
                   "from manifest id or start_url.";
    return;
  }

  if (!app->manifest_id().has_value()) {
    app->SetManifestId(manifest_id);
  } else if (app->manifest_id() != manifest_id) {
    DLOG(ERROR) << "ApplySyncDataToApp: existing manifest_id doesn't match "
                   "manifest_id.";
    return;
  }

  if (app->start_url().is_empty()) {
    app->SetStartUrl(std::move(start_url));
  } else if (app->start_url() != start_url) {
    DLOG(ERROR)
        << "ApplySyncDataToApp: existing start_url doesn't match start_url.";
    return;
  }

  // Always override user_display mode with a synced value.
  app->SetUserDisplayMode(ToMojomDisplayMode(sync_data.user_display_mode()));
  app->SetUserPageOrdinal(syncer::StringOrdinal(sync_data.user_page_ordinal()));
  app->SetUserLaunchOrdinal(
      syncer::StringOrdinal(sync_data.user_launch_ordinal()));

  absl::optional<WebApp::SyncFallbackData> parsed_sync_fallback_data =
      ParseSyncFallbackDataStruct(sync_data);
  if (!parsed_sync_fallback_data.has_value()) {
    // ParseSyncFallbackDataStruct() reports any errors.
    return;
  }
  app->SetSyncFallbackData(std::move(parsed_sync_fallback_data.value()));
}

WebAppSyncBridge::WebAppSyncBridge(
    AbstractWebAppDatabaseFactory* database_factory,
    WebAppRegistrarMutable* registrar,
    SyncInstallDelegate* install_delegate)
    : WebAppSyncBridge(
          database_factory,
          registrar,
          install_delegate,
          std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
              syncer::WEB_APPS,
              base::BindRepeating(&syncer::ReportUnrecoverableError,
                                  chrome::GetChannel()))) {}

WebAppSyncBridge::WebAppSyncBridge(
    AbstractWebAppDatabaseFactory* database_factory,
    WebAppRegistrarMutable* registrar,
    SyncInstallDelegate* install_delegate,
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor)
    : syncer::ModelTypeSyncBridge(std::move(change_processor)),
      registrar_(registrar),
      install_delegate_(install_delegate) {
  DCHECK(database_factory);
  DCHECK(registrar_);
  database_ = std::make_unique<WebAppDatabase>(
      database_factory,
      base::BindRepeating(&WebAppSyncBridge::ReportErrorToChangeProcessor,
                          base::Unretained(this)));
}

WebAppSyncBridge::~WebAppSyncBridge() = default;

std::unique_ptr<WebAppRegistryUpdate> WebAppSyncBridge::BeginUpdate() {
  DCHECK(database_->is_opened());

  DCHECK(!is_in_update_);
  is_in_update_ = true;

  return std::make_unique<WebAppRegistryUpdate>(
      registrar_, base::PassKey<WebAppSyncBridge>());
}

void WebAppSyncBridge::CommitUpdate(
    std::unique_ptr<WebAppRegistryUpdate> update,
    CommitCallback callback) {
  DCHECK(is_in_update_);
  is_in_update_ = false;

  if (update == nullptr || update->update_data().IsEmpty()) {
    std::move(callback).Run(/*success*/ true);
    return;
  }

  CheckRegistryUpdateData(update->update_data());

  std::unique_ptr<RegistryUpdateData> update_data = update->TakeUpdateData();
  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      CreateMetadataChangeList();

  UpdateSync(*update_data, metadata_change_list.get());

  database_->Write(
      *update_data, std::move(metadata_change_list),
      base::BindOnce(&WebAppSyncBridge::OnDataWritten,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  UpdateRegistrar(std::move(update_data));
}

void WebAppSyncBridge::Init(base::OnceClosure callback) {
  database_->OpenDatabase(base::BindOnce(&WebAppSyncBridge::OnDatabaseOpened,
                                         weak_ptr_factory_.GetWeakPtr(),
                                         std::move(callback)));
}

void WebAppSyncBridge::SetAppUserDisplayMode(const AppId& app_id,
                                             DisplayMode user_display_mode,
                                             bool is_user_action) {
  if (is_user_action) {
    switch (user_display_mode) {
      case DisplayMode::kStandalone:
        base::RecordAction(
            base::UserMetricsAction("WebApp.SetWindowMode.Window"));
        break;
      case DisplayMode::kBrowser:
        base::RecordAction(base::UserMetricsAction("WebApp.SetWindowMode.Tab"));
        break;
      case DisplayMode::kTabbed:
        base::RecordAction(
            base::UserMetricsAction("WebApp.SetWindowMode.Tabbed"));
        break;
      default:
        NOTREACHED();
    }
  }

  ScopedRegistryUpdate update(this);
  WebApp* web_app = update->UpdateApp(app_id);
  if (web_app)
    web_app->SetUserDisplayMode(user_display_mode);

  registrar_->NotifyWebAppUserDisplayModeChanged(app_id, user_display_mode);
}

void WebAppSyncBridge::SetAppRunOnOsLoginMode(const AppId& app_id,
                                              RunOnOsLoginMode mode) {
  ScopedRegistryUpdate update(this);
  WebApp* web_app = update->UpdateApp(app_id);
  if (web_app)
    web_app->SetRunOnOsLoginMode(mode);
}
void WebAppSyncBridge::SetAppWindowControlsOverlayEnabled(const AppId& app_id,
                                                          bool enabled) {
  ScopedRegistryUpdate update(this);
  WebApp* web_app = update->UpdateApp(app_id);
  if (web_app)
    web_app->SetWindowControlsOverlayEnabled(enabled);
}

void WebAppSyncBridge::SetAppIsDisabled(const AppId& app_id, bool is_disabled) {
  if (!IsChromeOsDataMandatory())
    return;

  bool notify = false;
  {
    ScopedRegistryUpdate update(this);
    WebApp* web_app = update->UpdateApp(app_id);
    if (!web_app)
      return;

    absl::optional<WebAppChromeOsData> cros_data = web_app->chromeos_data();
    DCHECK(cros_data.has_value());

    if (cros_data->is_disabled != is_disabled) {
      cros_data->is_disabled = is_disabled;
      web_app->SetWebAppChromeOsData(std::move(cros_data));
      notify = true;
    }
  }

  if (notify)
    registrar_->NotifyWebAppDisabledStateChanged(app_id, is_disabled);
}

void WebAppSyncBridge::UpdateAppsDisableMode() {
  if (!IsChromeOsDataMandatory())
    return;

  registrar_->NotifyWebAppsDisabledModeChanged();
}

void WebAppSyncBridge::SetAppIsLocallyInstalled(const AppId& app_id,
                                                bool is_locally_installed) {
  {
    ScopedRegistryUpdate update(this);
    WebApp* web_app = update->UpdateApp(app_id);
    if (web_app)
      web_app->SetIsLocallyInstalled(is_locally_installed);
  }
  registrar_->NotifyWebAppLocallyInstalledStateChanged(app_id,
                                                       is_locally_installed);
}

void WebAppSyncBridge::SetAppLastBadgingTime(const AppId& app_id,
                                             const base::Time& time) {
  {
    ScopedRegistryUpdate update(this);
    WebApp* web_app = update->UpdateApp(app_id);
    if (web_app)
      web_app->SetLastBadgingTime(time);
  }
  registrar_->NotifyWebAppLastBadgingTimeChanged(app_id, time);
}

void WebAppSyncBridge::SetAppLastLaunchTime(const AppId& app_id,
                                            const base::Time& time) {
  {
    ScopedRegistryUpdate update(this);
    WebApp* web_app = update->UpdateApp(app_id);
    if (web_app)
      web_app->SetLastLaunchTime(time);
  }
  registrar_->NotifyWebAppLastLaunchTimeChanged(app_id, time);
}

void WebAppSyncBridge::SetAppInstallTime(const AppId& app_id,
                                         const base::Time& time) {
  {
    ScopedRegistryUpdate update(this);
    WebApp* web_app = update->UpdateApp(app_id);
    if (web_app)
      web_app->SetInstallTime(time);
  }
  registrar_->NotifyWebAppInstallTimeChanged(app_id, time);
}

void WebAppSyncBridge::SetAppManifestUpdateTime(const AppId& app_id,
                                                const base::Time& time) {
  {
    ScopedRegistryUpdate update(this);
    WebApp* web_app = update->UpdateApp(app_id);
    if (web_app)
      web_app->SetManifestUpdateTime(time);
  }
}

void WebAppSyncBridge::SetUserPageOrdinal(const AppId& app_id,
                                          syncer::StringOrdinal page_ordinal) {
  ScopedRegistryUpdate update(this);
  WebApp* web_app = update->UpdateApp(app_id);
  // Due to the extensions sync system setting ordinals on sync, this can get
  // called before the app is installed in the web apps system. Until apps are
  // no longer double-installed on both systems, ignore this case.
  // https://crbug.com/1101781
  if (!registrar_->IsInstalled(app_id))
    return;
  if (web_app)
    web_app->SetUserPageOrdinal(page_ordinal);
}

void WebAppSyncBridge::SetUserLaunchOrdinal(
    const AppId& app_id,
    syncer::StringOrdinal launch_ordinal) {
  ScopedRegistryUpdate update(this);
  // Due to the extensions sync system setting ordinals on sync, this can get
  // called before the app is installed in the web apps system. Until apps are
  // no longer double-installed on both systems, ignore this case.
  // https://crbug.com/1101781
  if (!registrar_->IsInstalled(app_id))
    return;
  WebApp* web_app = update->UpdateApp(app_id);
  if (web_app)
    web_app->SetUserLaunchOrdinal(launch_ordinal);
}

void WebAppSyncBridge::AddAllowedLaunchProtocol(
    const AppId& app_id,
    const std::string& protocol_scheme) {
  // Use a scope here, so that the web app registry is updated when
  // `update` goes out of scope. If it doesn't then observers will
  // examine stale data.
  {
    ScopedRegistryUpdate update(this);
    web_app::WebApp* app_to_update = update->UpdateApp(app_id);
    base::flat_set<std::string> protocol_handlers(
        app_to_update->allowed_launch_protocols());

    DCHECK(!base::Contains(protocol_handlers, protocol_scheme));
    protocol_handlers.insert(protocol_scheme);
    app_to_update->SetAllowedLaunchProtocols(std::move(protocol_handlers));
  }
  // Notify observers that the list of allowed protocols was updated.
  registrar_->NotifyWebAppProtocolSettingsChanged();
}

void WebAppSyncBridge::RemoveAllowedLaunchProtocol(
    const AppId& app_id,
    const std::string& protocol_scheme) {
  // Use a scope here, so that the web app registry is updated when
  // `update` goes out of scope. If it doesn't then observers will
  // examine stale data.
  {
    ScopedRegistryUpdate update(this);
    web_app::WebApp* app_to_update = update->UpdateApp(app_id);
    base::flat_set<std::string> protocol_handlers(
        app_to_update->allowed_launch_protocols());
    protocol_handlers.erase(protocol_scheme);
    app_to_update->SetAllowedLaunchProtocols(std::move(protocol_handlers));
  }
  // Notify observers that the list of allowed protocols was updated.
  registrar_->NotifyWebAppProtocolSettingsChanged();
}

void WebAppSyncBridge::AddDisallowedLaunchProtocol(
    const AppId& app_id,
    const std::string& protocol_scheme) {
  // Use a scope here, so that the web app registry is updated when
  // `update` goes out of scope. If it doesn't then observers will
  // examine stale data.
  {
    ScopedRegistryUpdate update(this);
    web_app::WebApp* app_to_update = update->UpdateApp(app_id);
    base::flat_set<std::string> protocol_handlers(
        app_to_update->disallowed_launch_protocols());

    DCHECK(!base::Contains(protocol_handlers, protocol_scheme));
    protocol_handlers.insert(protocol_scheme);
    app_to_update->SetDisallowedLaunchProtocols(std::move(protocol_handlers));
  }
  // Notify observers that the list of disallowed protocols was updated.
  registrar_->NotifyWebAppProtocolSettingsChanged();
}

void WebAppSyncBridge::RemoveDisallowedLaunchProtocol(
    const AppId& app_id,
    const std::string& protocol_scheme) {
  // Use a scope here, so that the web app registry is updated when
  // `update` goes out of scope. If it doesn't then observers will
  // examine stale data.
  {
    ScopedRegistryUpdate update(this);
    web_app::WebApp* app_to_update = update->UpdateApp(app_id);
    base::flat_set<std::string> protocol_handlers(
        app_to_update->disallowed_launch_protocols());
    protocol_handlers.erase(protocol_scheme);
    app_to_update->SetDisallowedLaunchProtocols(std::move(protocol_handlers));
  }
  // Notify observers that the list of disallowed protocols was updated.
  registrar_->NotifyWebAppProtocolSettingsChanged();
}

void WebAppSyncBridge::SetAppFileHandlerApprovalState(const AppId& app_id,
                                                      ApiApprovalState state) {
  {
    ScopedRegistryUpdate(this)->UpdateApp(app_id)->SetFileHandlerApprovalState(
        state);
  }
  registrar_->NotifyWebAppFileHandlerApprovalStateChanged(app_id);
}

void WebAppSyncBridge::CheckRegistryUpdateData(
    const RegistryUpdateData& update_data) const {
#if DCHECK_IS_ON()
  for (const std::unique_ptr<WebApp>& web_app : update_data.apps_to_create) {
    DCHECK(!registrar_->GetAppById(web_app->app_id()));
    DCHECK(!web_app->name().empty());
  }

  for (const std::unique_ptr<WebApp>& web_app : update_data.apps_to_update) {
    DCHECK(registrar_->GetAppById(web_app->app_id()));
    DCHECK(!web_app->name().empty());
  }

  for (const AppId& app_id : update_data.apps_to_delete)
    DCHECK(registrar_->GetAppById(app_id));
#endif
}

std::vector<std::unique_ptr<WebApp>> WebAppSyncBridge::UpdateRegistrar(
    std::unique_ptr<RegistryUpdateData> update_data) {
  registrar_->CountMutation();

  std::vector<std::unique_ptr<WebApp>> apps_unregistered;

  for (std::unique_ptr<WebApp>& web_app : update_data->apps_to_create) {
    AppId app_id = web_app->app_id();
    DCHECK(!registrar_->GetAppById(app_id));
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // We do not install non-system web apps in Ash when Lacros web apps are
    // enabled.
    DCHECK(web_app->IsSystemApp() || !IsWebAppsCrosapiEnabled());
#endif
    registrar_->registry().emplace(std::move(app_id), std::move(web_app));
  }

  for (std::unique_ptr<WebApp>& web_app : update_data->apps_to_update) {
    WebApp* original_web_app = registrar_->GetAppByIdMutable(web_app->app_id());
    DCHECK(original_web_app);
    DCHECK_EQ(web_app->IsSystemApp(), original_web_app->IsSystemApp());
    // Commit previously created copy into original. Preserve original web_app
    // object pointer value (the object's identity) to support stored pointers.
    *original_web_app = std::move(*web_app);
  }

  for (const AppId& app_id : update_data->apps_to_delete) {
    auto it = registrar_->registry().find(app_id);
    DCHECK(it != registrar_->registry().end());

    apps_unregistered.push_back(std::move(it->second));
    registrar_->registry().erase(it);
  }

  return apps_unregistered;
}

void WebAppSyncBridge::UpdateSync(
    const RegistryUpdateData& update_data,
    syncer::MetadataChangeList* metadata_change_list) {
  // We don't block web app subsystems on WebAppSyncBridge::MergeSyncData: we
  // call WebAppProvider::OnRegistryControllerReady() right after
  // change_processor()->ModelReadyToSync. As a result, subsystems may produce
  // some local changes between OnRegistryControllerReady and MergeSyncData.
  // Return early in this case. The processor cannot do any useful metadata
  // tracking until MergeSyncData is called:
  if (!change_processor()->IsTrackingMetadata())
    return;

  for (const std::unique_ptr<WebApp>& new_app : update_data.apps_to_create) {
    if (new_app->IsSynced()) {
      change_processor()->Put(new_app->app_id(), CreateSyncEntityData(*new_app),
                              metadata_change_list);
    }
  }

  for (const std::unique_ptr<WebApp>& new_state : update_data.apps_to_update) {
    const AppId& app_id = new_state->app_id();
    // Find the current state of the app to be overritten.
    const WebApp* current_state = registrar_->GetAppById(app_id);
    DCHECK(current_state);

    // Include the app in the sync "view" if IsSynced flag becomes true. Update
    // the app if IsSynced flag stays true. Exclude the app from the sync "view"
    // if IsSynced flag becomes false.
    //
    // TODO(loyso): Send an update to sync server only if any sync-specific
    // data was changed. Implement some dirty flags in WebApp setter methods.
    if (new_state->IsSynced()) {
      change_processor()->Put(app_id, CreateSyncEntityData(*new_state),
                              metadata_change_list);
    } else if (current_state->IsSynced()) {
      change_processor()->Delete(app_id, metadata_change_list);
    }
  }

  for (const AppId& app_id_to_delete : update_data.apps_to_delete) {
    const WebApp* current_state = registrar_->GetAppById(app_id_to_delete);
    DCHECK(current_state);
    // Exclude the app from the sync "view" if IsSynced flag was true.
    if (current_state->IsSynced())
      change_processor()->Delete(app_id_to_delete, metadata_change_list);
  }
}

void WebAppSyncBridge::OnDatabaseOpened(
    base::OnceClosure callback,
    Registry registry,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  DCHECK(database_->is_opened());

  // Provide sync metadata to the processor _before_ any local changes occur.
  change_processor()->ModelReadyToSync(std::move(metadata_batch));

  registrar_->InitRegistry(std::move(registry));
  std::move(callback).Run();

  MaybeUninstallAppsPendingUninstall();
  MaybeInstallAppsFromSyncAndPendingInstallation();
}

void WebAppSyncBridge::OnDataWritten(CommitCallback callback, bool success) {
  if (!success)
    DLOG(ERROR) << "WebAppSyncBridge commit failed";

  base::UmaHistogramBoolean("WebApp.Database.WriteResult", success);
  std::move(callback).Run(success);
}

void WebAppSyncBridge::WebAppUninstalled(const AppId& app, bool uninstalled) {
  // In the case `uninstalled` is false, the AppId should still be removed from
  // the set, since uninstall failures are not yet handled, and there are no
  // uninstall retry attempts.
  apps_in_sync_uninstall_.erase(app);
}

void WebAppSyncBridge::ReportErrorToChangeProcessor(
    const syncer::ModelError& error) {
  change_processor()->ReportError(error);
}

void WebAppSyncBridge::MergeLocalAppsToSync(
    const syncer::EntityChangeList& entity_data,
    syncer::MetadataChangeList* metadata_change_list) {
  auto sync_server_apps = base::MakeFlatSet<AppId>(
      entity_data, {}, &syncer::EntityChange::storage_key);

  for (const WebApp& app : registrar_->GetAppsIncludingStubs()) {
    if (!app.IsSynced())
      continue;

    bool exists_remotely = sync_server_apps.contains(app.app_id());
    if (!exists_remotely) {
      change_processor()->Put(app.app_id(), CreateSyncEntityData(app),
                              metadata_change_list);
    }
  }
}

void WebAppSyncBridge::ApplySyncDataChange(
    const syncer::EntityChange& change,
    RegistryUpdateData* update_local_data) {
  // app_id is storage key.
  const AppId& app_id = change.storage_key();

  const WebApp* existing_web_app = registrar_->GetAppByIdMutable(app_id);

  // Handle deletion first.
  if (change.type() == syncer::EntityChange::ACTION_DELETE) {
    if (!existing_web_app) {
      DLOG(ERROR) << "ApplySyncDataChange error: no app to delete";
      return;
    }
    // Do copy on write:
    auto app_copy = std::make_unique<WebApp>(*existing_web_app);
    app_copy->RemoveSource(Source::kSync);

    if (app_copy->HasAnySources())
      update_local_data->apps_to_update.push_back(std::move(app_copy));
    else
      update_local_data->apps_to_delete.push_back(app_id);

    return;
  }

  // Handle EntityChange::ACTION_ADD and EntityChange::ACTION_UPDATE.
  DCHECK(change.data().specifics.has_web_app());
  const sync_pb::WebAppSpecifics& specifics = change.data().specifics.web_app();

  if (existing_web_app) {
    // Any entities that appear in both sets must be merged.
    // Do copy on write:
    auto app_copy = std::make_unique<WebApp>(*existing_web_app);
    ApplySyncDataToApp(specifics, app_copy.get());
    // Preserve web_app->is_locally_installed user's choice here.

    update_local_data->apps_to_update.push_back(std::move(app_copy));
  } else {
    // Any remote entities that don’t exist locally must be written to local
    // storage.
    auto web_app = std::make_unique<WebApp>(app_id);

    // Request a followup sync-initiated install for this stub app to fetch
    // full local data and all the icons.
    web_app->SetIsFromSyncAndPendingInstallation(true);

    // The sync system requires non-empty name, populate temp name from
    // the fallback sync data name:
    web_app->SetName(specifics.name());
    // Or use syncer::EntityData::name as a last resort.
    if (web_app->name().empty())
      web_app->SetName(change.data().name);

    ApplySyncDataToApp(specifics, web_app.get());

    // For a new app, automatically choose if we want to install it locally.
    web_app->SetIsLocallyInstalled(AreAppsLocallyInstalledBySync());

    update_local_data->apps_to_create.push_back(std::move(web_app));
  }
}

void WebAppSyncBridge::ApplySyncChangesToRegistrar(
    std::unique_ptr<RegistryUpdateData> update_local_data) {
  if (update_local_data->IsEmpty())
    return;

  // Notify observers that web apps will be updated.
  // Prepare a short living read-only "view" to support const correctness:
  // observers must not modify the |new_apps_state|.
  if (!update_local_data->apps_to_update.empty()) {
    std::vector<const WebApp*> new_apps_state;
    new_apps_state.reserve(update_local_data->apps_to_update.size());
    for (const std::unique_ptr<WebApp>& new_web_app_state :
         update_local_data->apps_to_update) {
      new_apps_state.push_back(new_web_app_state.get());
    }
    registrar_->NotifyWebAppsWillBeUpdatedFromSync(new_apps_state);
  }

  // Initiate any uninstall actions to clean up os integration, disk data, etc.
  // This starts before the registry is updated as some of these functions
  // require the web app data to still exist in the registry. This includes
  // eventually notifying observers of `WebAppWillBeUninstalled` and
  // `WebAppUninstalled`.
  const std::vector<AppId>& apps_to_delete = update_local_data->apps_to_delete;
  if (!apps_to_delete.empty()) {
    apps_in_sync_uninstall_.insert(apps_to_delete.begin(),
                                   apps_to_delete.end());
    install_delegate_->UninstallWithoutRegistryUpdateFromSync(
        apps_to_delete,
        base::BindRepeating(&WebAppSyncBridge::WebAppUninstalled,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  std::vector<WebApp*> apps_to_install;
  for (const auto& web_app : update_local_data->apps_to_create)
    apps_to_install.push_back(web_app.get());

  // TODO(dmurph): Determine if we these are needed anymore, and if not,
  // simplify the code to just delete them earlier.
  std::vector<std::unique_ptr<WebApp>> apps_unregistered =
      UpdateRegistrar(std::move(update_local_data));

  // Do a full follow up install for all remote entities that don’t exist
  // locally.
  if (!apps_to_install.empty()) {
    install_delegate_->InstallWebAppsAfterSync(std::move(apps_to_install),
                                               base::DoNothing());
  }
}

std::unique_ptr<syncer::MetadataChangeList>
WebAppSyncBridge::CreateMetadataChangeList() {
  return syncer::ModelTypeStore::WriteBatch::CreateMetadataChangeList();
}

absl::optional<syncer::ModelError> WebAppSyncBridge::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  CHECK(change_processor()->IsTrackingMetadata());

  auto update_local_data = std::make_unique<RegistryUpdateData>();

  for (const auto& change : entity_data) {
    DCHECK_NE(change->type(), syncer::EntityChange::ACTION_DELETE);
    ApplySyncDataChange(*change, update_local_data.get());
  }

  MergeLocalAppsToSync(entity_data, metadata_change_list.get());

  database_->Write(
      *update_local_data, std::move(metadata_change_list),
      base::BindOnce(&WebAppSyncBridge::OnDataWritten,
                     weak_ptr_factory_.GetWeakPtr(), base::DoNothing()));

  ApplySyncChangesToRegistrar(std::move(update_local_data));
  return absl::nullopt;
}

absl::optional<syncer::ModelError> WebAppSyncBridge::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  if (!disable_checks_for_testing_)
    CHECK(change_processor()->IsTrackingMetadata());

  auto update_local_data = std::make_unique<RegistryUpdateData>();

  for (const auto& change : entity_changes)
    ApplySyncDataChange(*change, update_local_data.get());

  database_->Write(
      *update_local_data, std::move(metadata_change_list),
      base::BindOnce(&WebAppSyncBridge::OnDataWritten,
                     weak_ptr_factory_.GetWeakPtr(), base::DoNothing()));

  ApplySyncChangesToRegistrar(std::move(update_local_data));
  return absl::nullopt;
}

void WebAppSyncBridge::GetData(StorageKeyList storage_keys,
                               DataCallback callback) {
  auto data_batch = std::make_unique<syncer::MutableDataBatch>();

  for (const AppId& app_id : storage_keys) {
    const WebApp* app = registrar_->GetAppById(app_id);
    if (app && app->IsSynced())
      data_batch->Put(app->app_id(), CreateSyncEntityData(*app));
  }

  std::move(callback).Run(std::move(data_batch));
}

void WebAppSyncBridge::GetAllDataForDebugging(DataCallback callback) {
  auto data_batch = std::make_unique<syncer::MutableDataBatch>();

  for (const WebApp& app : registrar_->GetAppsIncludingStubs()) {
    if (app.IsSynced())
      data_batch->Put(app.app_id(), CreateSyncEntityData(app));
  }

  std::move(callback).Run(std::move(data_batch));
}

std::string WebAppSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_web_app());

  const sync_pb::WebAppSpecifics& specifics = entity_data.specifics.web_app();
  const GURL start_url(specifics.start_url());
  if (start_url.is_empty() || !start_url.is_valid()) {
    DLOG(ERROR) << "GetClientTag: start_url parse error.";
    return std::string();
  }

  absl::optional<std::string> manifest_id = absl::nullopt;
  if (specifics.has_manifest_id())
    manifest_id = absl::optional<std::string>(specifics.manifest_id());
  return GenerateAppId(manifest_id, start_url);
}

std::string WebAppSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  return GetClientTag(entity_data);
}

const std::set<AppId>& WebAppSyncBridge::GetAppsInSyncUninstallForTest() {
  return apps_in_sync_uninstall_;
}

void WebAppSyncBridge::MaybeUninstallAppsPendingUninstall() {
  std::vector<AppId> apps_uninstalling;

  for (WebApp& app : registrar_->GetAppsIncludingStubsMutable()) {
    if (app.is_uninstalling())
      apps_uninstalling.push_back(app.app_id());
  }

  base::UmaHistogramCounts100("WebApp.Uninstall.NonSyncIncompleteCount",
                              apps_uninstalling.size());

  if (!apps_uninstalling.empty())
    install_delegate_->RetryIncompleteUninstalls(apps_uninstalling);
}

void WebAppSyncBridge::MaybeInstallAppsFromSyncAndPendingInstallation() {
  std::vector<WebApp*> apps_in_sync_install;

  for (WebApp& app : registrar_->GetAppsIncludingStubsMutable()) {
    if (app.is_from_sync_and_pending_installation())
      apps_in_sync_install.push_back(&app);
  }

  if (!apps_in_sync_install.empty()) {
    install_delegate_->InstallWebAppsAfterSync(std::move(apps_in_sync_install),
                                               base::DoNothing());
  }
}

}  // namespace web_app
