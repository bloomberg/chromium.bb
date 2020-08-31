// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_migration_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_database.h"
#include "chrome/browser/web_applications/web_app_database_factory.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_store.h"

namespace web_app {

WebAppMigrationManager::WebAppMigrationManager(
    Profile* profile,
    AbstractWebAppDatabaseFactory* database_factory,
    WebAppIconManager* web_app_icon_manager)
    : bookmark_app_registrar_(profile),
      bookmark_app_registry_controller_(profile, &bookmark_app_registrar_),
      bookmark_app_icon_manager_(profile),
      bookmark_app_file_handler_manager_(profile),
      database_factory_(database_factory),
      web_app_icon_manager_(web_app_icon_manager) {
  database_ = std::make_unique<WebAppDatabase>(
      database_factory_,
      base::BindRepeating(&WebAppMigrationManager::ReportDatabaseError,
                          base::Unretained(this)));
  bookmark_app_file_handler_manager_.SetSubsystems(&bookmark_app_registrar_);
}

WebAppMigrationManager::~WebAppMigrationManager() = default;

void WebAppMigrationManager::StartDatabaseMigration(
    MigrationCompletedCallback migration_completed_callback) {
  DCHECK(database_);
  migration_completed_callback_ = std::move(migration_completed_callback);

  // Open LevelDB first. The extension system behind
  // BookmarkAppRegistryController is already started in parallel.
  database_->OpenDatabase(
      base::BindOnce(&WebAppMigrationManager::OnWebAppDatabaseOpened,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebAppMigrationManager::OnWebAppDatabaseOpened(
    Registry web_app_registry,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  if (metadata_batch->GetModelTypeState().initial_sync_done()) {
    // If initial sync is done, then WebAppSyncBridge::MergeSyncData was already
    // called once. All the migrated entities are already listed in sync
    // metadata. Any additional apps from this point in time should be installed
    // via WebAppSyncBridge::CommitUpdate() "as if" a user installs them.
    ScheduleDestructDatabaseAndCallCallback(/*success=*/true);
    return;
  }

  // Wait for the Extensions System to be ready.
  bookmark_app_registry_controller_.Init(base::BindOnce(
      &WebAppMigrationManager::OnBookmarkAppRegistryReady,
      weak_ptr_factory_.GetWeakPtr(), std::move(web_app_registry)));
}

void WebAppMigrationManager::OnBookmarkAppRegistryReady(
    Registry web_app_registry) {
  bookmark_app_ids_ = bookmark_app_registrar_.GetAppIds();

  // Remove bookmark app ids already listed in the web app registry.
  base::EraseIf(bookmark_app_ids_, [&web_app_registry](const AppId& app_id) {
    return base::Contains(web_app_registry, app_id);
  });

  // Migrate icons first, the registry data (the LevelDB transaction) last.
  next_app_id_iterator_ = bookmark_app_ids_.begin();
  MigrateNextBookmarkAppIcons();
}

void WebAppMigrationManager::MigrateNextBookmarkAppIcons() {
  AppId app_id;
  do {
    if (next_app_id_iterator_ == bookmark_app_ids_.end()) {
      MigrateBookmarkAppsRegistry();
      return;
    }

    app_id = *next_app_id_iterator_;
    ++next_app_id_iterator_;
  } while (!CanMigrateBookmarkApp(app_id));

  bookmark_app_icon_manager_.ReadAllIcons(
      app_id, base::BindOnce(&WebAppMigrationManager::OnBookmarkAppIconsRead,
                             weak_ptr_factory_.GetWeakPtr(), app_id));
}

void WebAppMigrationManager::OnBookmarkAppIconsRead(
    const AppId& app_id,
    std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
  if (icon_bitmaps.empty()) {
    DLOG(ERROR) << "Read bookmark app icons failed.";
    MigrateNextBookmarkAppIcons();
    return;
  }
  // TODO(https://crbug.com/1069316): Support jump lists migration here: Convert
  // old extension's representation to new web app representation (project BMO).
  web_app_icon_manager_->WriteData(
      app_id, std::move(icon_bitmaps),
      std::vector<std::map<SquareSizePx, SkBitmap>>(),
      base::BindOnce(&WebAppMigrationManager::OnWebAppIconsWritten,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebAppMigrationManager::OnWebAppIconsWritten(bool success) {
  if (!success)
    DLOG(ERROR) << "Write web app icons failed.";

  MigrateNextBookmarkAppIcons();
}

void WebAppMigrationManager::MigrateBookmarkAppInstallSource(
    const AppId& app_id,
    WebApp* web_app) {
  bool is_arc = bookmark_app_registrar_.HasExternalAppWithInstallSource(
      app_id, ExternalInstallSource::kArc);

  bool is_policy = bookmark_app_registrar_.HasExternalAppWithInstallSource(
      app_id, ExternalInstallSource::kExternalPolicy);

  bool is_default = bookmark_app_registrar_.HasExternalAppWithInstallSource(
                        app_id, ExternalInstallSource::kInternalDefault) ||
                    bookmark_app_registrar_.HasExternalAppWithInstallSource(
                        app_id, ExternalInstallSource::kExternalDefault);

  bool is_system = bookmark_app_registrar_.HasExternalAppWithInstallSource(
      app_id, ExternalInstallSource::kSystemInstalled);

  if (is_default)
    web_app->AddSource(Source::kDefault);

  if (is_policy)
    web_app->AddSource(Source::kPolicy);

  if (is_system)
    web_app->AddSource(Source::kSystem);

  if (is_arc)
    web_app->AddSource(Source::kWebAppStore);

  if (!bookmark_app_registrar_.HasExternalApp(app_id))
    web_app->AddSource(Source::kSync);

  DCHECK(web_app->HasAnySources());
}

bool WebAppMigrationManager::CanMigrateBookmarkApp(const AppId& app_id) const {
  if (!bookmark_app_registrar_.IsInstalled(app_id))
    return false;

  // SystemWebAppManager will re-install these.
  if (bookmark_app_registrar_.HasExternalAppWithInstallSource(
          app_id, ExternalInstallSource::kSystemInstalled)) {
    return false;
  }

  GURL launch_url = bookmark_app_registrar_.GetAppLaunchURL(app_id);
  return GenerateAppIdFromURL(launch_url) == app_id;
}

std::unique_ptr<WebApp> WebAppMigrationManager::MigrateBookmarkApp(
    const AppId& app_id) {
  DCHECK(CanMigrateBookmarkApp(app_id));

  auto web_app = std::make_unique<WebApp>(app_id);

  web_app->SetName(bookmark_app_registrar_.GetAppShortName(app_id));
  web_app->SetDescription(bookmark_app_registrar_.GetAppDescription(app_id));
  web_app->SetLaunchUrl(bookmark_app_registrar_.GetAppLaunchURL(app_id));

  base::Optional<GURL> scope = bookmark_app_registrar_.GetAppScope(app_id);
  if (scope)
    web_app->SetScope(*scope);

  web_app->SetThemeColor(bookmark_app_registrar_.GetAppThemeColor(app_id));
  web_app->SetDisplayMode(bookmark_app_registrar_.GetAppDisplayMode(app_id));

  DisplayMode user_display_mode =
      bookmark_app_registrar_.GetAppUserDisplayMode(app_id);
  if (user_display_mode != DisplayMode::kUndefined)
    web_app->SetUserDisplayMode(user_display_mode);

  web_app->SetIsLocallyInstalled(
      bookmark_app_registrar_.IsLocallyInstalled(app_id));
  web_app->SetIconInfos(bookmark_app_registrar_.GetAppIconInfos(app_id));
  web_app->SetDownloadedIconSizes(
      bookmark_app_registrar_.GetAppDownloadedIconSizes(app_id));

  WebApp::SyncData sync_data;
  sync_data.name = bookmark_app_registrar_.GetAppShortName(app_id);
  sync_data.theme_color = bookmark_app_registrar_.GetAppThemeColor(app_id);
  web_app->SetSyncData(std::move(sync_data));

  const apps::FileHandlers* file_handlers =
      bookmark_app_file_handler_manager_.GetAllFileHandlers(app_id);
  if (file_handlers)
    web_app->SetFileHandlers(*file_handlers);

  MigrateBookmarkAppInstallSource(app_id, web_app.get());

  return web_app;
}

void WebAppMigrationManager::MigrateBookmarkAppsRegistry() {
  DCHECK(database_);

  if (bookmark_app_ids_.empty()) {
    ScheduleDestructDatabaseAndCallCallback(/*success=*/true);
    return;
  }

  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      syncer::ModelTypeStore::WriteBatch::CreateMetadataChangeList();

  RegistryUpdateData update_data;
  update_data.apps_to_create.reserve(bookmark_app_ids_.size());

  for (const AppId& app_id : bookmark_app_ids_) {
    if (CanMigrateBookmarkApp(app_id)) {
      std::unique_ptr<WebApp> web_app = MigrateBookmarkApp(app_id);
      update_data.apps_to_create.push_back(std::move(web_app));
    }
  }

  database_->Write(
      update_data, std::move(metadata_change_list),
      base::BindOnce(&WebAppMigrationManager::OnWebAppRegistryWritten,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebAppMigrationManager::OnWebAppRegistryWritten(bool success) {
  if (!success)
    DLOG(ERROR) << "Web app registry commit failed.";

  ScheduleDestructDatabaseAndCallCallback(success);
}

void WebAppMigrationManager::ReportDatabaseError(
    const syncer::ModelError& error) {
  DLOG(ERROR) << "Web app database error. " << error.ToString();

  ScheduleDestructDatabaseAndCallCallback(/*success=*/false);
}

void WebAppMigrationManager::ScheduleDestructDatabaseAndCallCallback(
    bool success) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&WebAppMigrationManager::DestructDatabaseAndCallCallback,
                     weak_ptr_factory_.GetWeakPtr(), success));
}

void WebAppMigrationManager::DestructDatabaseAndCallCallback(bool success) {
  // Close the database.
  database_ = nullptr;

  if (migration_completed_callback_)
    std::move(migration_completed_callback_).Run(success);
}

}  // namespace web_app
