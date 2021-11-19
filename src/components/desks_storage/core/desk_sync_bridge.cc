// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/desk_sync_bridge.h"

#include <algorithm>

#include "ash/public/cpp/desk_template.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chromeos/ui/base/window_state_type.h"
#include "components/account_id/account_id.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/window_info.h"
#include "components/desks_storage/core/desk_model_observer.h"
#include "components/desks_storage/core/desk_template_conversion.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/protocol/workspace_desk_specifics.pb.h"
#include "extensions/common/constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/ui_base_types.h"

namespace desks_storage {

using BrowserAppTab =
    sync_pb::WorkspaceDeskSpecifics_BrowserAppWindow_BrowserAppTab;
using BrowserAppWindow = sync_pb::WorkspaceDeskSpecifics_BrowserAppWindow;
using ash::DeskTemplate;
using ash::DeskTemplateSource;
using WindowState = sync_pb::WorkspaceDeskSpecifics_WindowState;
using WindowBound = sync_pb::WorkspaceDeskSpecifics_WindowBound;
using ProgressiveWebApp = sync_pb::WorkspaceDeskSpecifics_ProgressiveWebApp;
using ChromeApp = sync_pb::WorkspaceDeskSpecifics_ChromeApp;
using WorkspaceDeskSpecifics_App = sync_pb::WorkspaceDeskSpecifics_App;

namespace {

using syncer::ModelTypeStore;

// The maximum number of templates the local storage can hold.
constexpr std::size_t kMaxTemplateCount = 6u;

// Allocate a EntityData and copies |specifics| into it.
std::unique_ptr<syncer::EntityData> CopyToEntityData(
    const sync_pb::WorkspaceDeskSpecifics& specifics) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  *entity_data->specifics.mutable_workspace_desk() = specifics;
  entity_data->name = specifics.uuid();
  entity_data->creation_time = desk_template_conversion::ProtoTimeToTime(
      specifics.created_time_windows_epoch_micros());
  return entity_data;
}

// Parses the content of |record_list| into |*desk_templates|.
absl::optional<syncer::ModelError> ParseDeskTemplatesOnBackendSequence(
    std::map<base::GUID, std::unique_ptr<DeskTemplate>>* desk_templates,
    std::unique_ptr<ModelTypeStore::RecordList> record_list) {
  DCHECK(desk_templates);
  DCHECK(desk_templates->empty());
  DCHECK(record_list);

  for (const syncer::ModelTypeStore::Record& r : *record_list) {
    auto specifics = std::make_unique<sync_pb::WorkspaceDeskSpecifics>();
    if (specifics->ParseFromString(r.value)) {
      const base::GUID uuid =
          base::GUID::ParseCaseInsensitive(specifics->uuid());
      if (!uuid.is_valid()) {
        return syncer::ModelError(
            FROM_HERE,
            base::StringPrintf("Failed to parse WorkspaceDeskSpecifics uuid %s",
                               specifics->uuid().c_str()));
      }

      std::unique_ptr<ash::DeskTemplate> entry =
          DeskSyncBridge::FromSyncProto(*specifics);

      if (!entry)
        continue;

      (*desk_templates)[uuid] = std::move(entry);
    } else {
      return syncer::ModelError(
          FROM_HERE, "Failed to deserialize WorkspaceDeskSpecifics.");
    }
  }

  return absl::nullopt;
}

// Fill |out_gurls| using tabs' URL in |browser_app_window|.
void FillUrlList(std::vector<GURL>* out_gurls,
                 const BrowserAppWindow& browser_app_window) {
  for (auto tab : browser_app_window.tabs()) {
    if (tab.has_url())
      out_gurls->emplace_back(tab.url());
  }
}

// Get App ID from App proto.
std::string GetAppId(const sync_pb::WorkspaceDeskSpecifics_App& app) {
  switch (app.app().app_case()) {
    case sync_pb::WorkspaceDeskSpecifics_AppOneOf::AppCase::APP_NOT_SET:
      // Return an empty string to indicate this app is unsupported.
      return std::string();
    case sync_pb::WorkspaceDeskSpecifics_AppOneOf::AppCase::kBrowserAppWindow:
      // Browser app has a known app ID.
      return std::string(extension_misc::kChromeAppId);
    case sync_pb::WorkspaceDeskSpecifics_AppOneOf::AppCase::kChromeApp:
      return app.app().chrome_app().app_id();
    case sync_pb::WorkspaceDeskSpecifics_AppOneOf::AppCase::kProgressWebApp:
      return app.app().progress_web_app().app_id();
      // Leave out the default case to let compiler to ensure we have
      // exhaustively handled all cases.
  }
}

// Convert App proto to |app_restore::AppLaunchInfo|.
std::unique_ptr<app_restore::AppLaunchInfo> ConvertToAppLaunchInfo(
    const sync_pb::WorkspaceDeskSpecifics_App& app) {
  const int32_t window_id = app.window_id();
  const std::string app_id = GetAppId(app);

  if (app_id.empty())
    return nullptr;

  std::unique_ptr<app_restore::AppLaunchInfo> app_launch_info =
      std::make_unique<app_restore::AppLaunchInfo>(app_id, window_id);

  if (app.has_display_id())
    app_launch_info->display_id = app.display_id();

  switch (app.app().app_case()) {
    case sync_pb::WorkspaceDeskSpecifics_AppOneOf::AppCase::APP_NOT_SET:
      // This should never happen. |APP_NOT_SET| corresponds to empty |app_id|.
      // This method will early return when |app_id| is empty.
      NOTREACHED();
      break;
    case sync_pb::WorkspaceDeskSpecifics_AppOneOf::AppCase::kBrowserAppWindow:
      if (app.app().browser_app_window().has_active_tab_index()) {
        app_launch_info->active_tab_index =
            app.app().browser_app_window().active_tab_index();
      }

      app_launch_info->urls.emplace();
      FillUrlList(&app_launch_info->urls.value(),
                  app.app().browser_app_window());

      break;
    case sync_pb::WorkspaceDeskSpecifics_AppOneOf::AppCase::kChromeApp:
      // |app_id| is enough to identify a Chrome app.
      break;
    case sync_pb::WorkspaceDeskSpecifics_AppOneOf::AppCase::kProgressWebApp:
      // |app_id| is enough to identify a Progressive Web app.
      break;
      // Leave out the default case to let compiler to ensure we have
      // exhaustively handled all cases.
  }

  return app_launch_info;
}

// Convert Sync proto WindowState |state| to ui::WindowShowState used by
// the app_restore::WindowInfo struct.
ui::WindowShowState ToUiWindowState(WindowState state) {
  switch (state) {
    case WindowState::WorkspaceDeskSpecifics_WindowState_UNKNOWN_WINDOW_STATE:
      return ui::WindowShowState::SHOW_STATE_NORMAL;
    case WindowState::WorkspaceDeskSpecifics_WindowState_NORMAL:
      return ui::WindowShowState::SHOW_STATE_NORMAL;
    case WindowState::WorkspaceDeskSpecifics_WindowState_MINIMIZED:
      return ui::WindowShowState::SHOW_STATE_MINIMIZED;
    case WindowState::WorkspaceDeskSpecifics_WindowState_MAXIMIZED:
      return ui::WindowShowState::SHOW_STATE_MAXIMIZED;
    case WindowState::WorkspaceDeskSpecifics_WindowState_FULLSCREEN:
      return ui::WindowShowState::SHOW_STATE_FULLSCREEN;
    case WindowState::WorkspaceDeskSpecifics_WindowState_PRIMARY_SNAPPED:
      return ui::WindowShowState::SHOW_STATE_NORMAL;
    case WindowState::WorkspaceDeskSpecifics_WindowState_SECONDARY_SNAPPED:
      return ui::WindowShowState::SHOW_STATE_NORMAL;
      // Leave out the default case to let compiler to ensure we have
      // exhaustively handled all cases.
  }
}

// Convert Sync proto WindowState |state| to chromeos::WindowStateType used by
// the app_restore::WindowInfo struct.
chromeos::WindowStateType ToChromeOsWindowState(WindowState state) {
  switch (state) {
    case WindowState::WorkspaceDeskSpecifics_WindowState_UNKNOWN_WINDOW_STATE:
      return chromeos::WindowStateType::kNormal;
    case WindowState::WorkspaceDeskSpecifics_WindowState_NORMAL:
      return chromeos::WindowStateType::kNormal;
    case WindowState::WorkspaceDeskSpecifics_WindowState_MINIMIZED:
      return chromeos::WindowStateType::kMinimized;
    case WindowState::WorkspaceDeskSpecifics_WindowState_MAXIMIZED:
      return chromeos::WindowStateType::kMaximized;
    case WindowState::WorkspaceDeskSpecifics_WindowState_FULLSCREEN:
      return chromeos::WindowStateType::kFullscreen;
    case WindowState::WorkspaceDeskSpecifics_WindowState_PRIMARY_SNAPPED:
      return chromeos::WindowStateType::kPrimarySnapped;
    case WindowState::WorkspaceDeskSpecifics_WindowState_SECONDARY_SNAPPED:
      return chromeos::WindowStateType::kSecondarySnapped;
      // Leave out the default case to let compiler to ensure we have
      // exhaustively handled all cases.
  }
}

// Convert chromeos::WindowStateType to Sync proto WindowState.
WindowState FromChromeOsWindowState(chromeos::WindowStateType state) {
  switch (state) {
    case chromeos::WindowStateType::kDefault:
    case chromeos::WindowStateType::kNormal:
    case chromeos::WindowStateType::kInactive:
    case chromeos::WindowStateType::kAutoPositioned:
    case chromeos::WindowStateType::kPinned:
    case chromeos::WindowStateType::kTrustedPinned:
    case chromeos::WindowStateType::kPip:
      return WindowState::WorkspaceDeskSpecifics_WindowState_NORMAL;
    case chromeos::WindowStateType::kMinimized:
      return WindowState::WorkspaceDeskSpecifics_WindowState_MINIMIZED;
    case chromeos::WindowStateType::kMaximized:
      return WindowState::WorkspaceDeskSpecifics_WindowState_MAXIMIZED;
    case chromeos::WindowStateType::kFullscreen:
      return WindowState::WorkspaceDeskSpecifics_WindowState_FULLSCREEN;
    case chromeos::WindowStateType::kPrimarySnapped:
      return WindowState::WorkspaceDeskSpecifics_WindowState_PRIMARY_SNAPPED;
    case chromeos::WindowStateType::kSecondarySnapped:
      return WindowState::WorkspaceDeskSpecifics_WindowState_SECONDARY_SNAPPED;
      // Leave out the default case to let compiler to ensure we have
      // exhaustively handled all cases.
  }
}

// Convert ui::WindowShowState to Sync proto WindowState.
WindowState FromUiWindowState(ui::WindowShowState state) {
  switch (state) {
    case ui::WindowShowState::SHOW_STATE_DEFAULT:
    case ui::WindowShowState::SHOW_STATE_NORMAL:
    case ui::WindowShowState::SHOW_STATE_INACTIVE:
    case ui::WindowShowState::SHOW_STATE_END:
      return WindowState::WorkspaceDeskSpecifics_WindowState_NORMAL;
    case ui::WindowShowState::SHOW_STATE_MINIMIZED:
      return WindowState::WorkspaceDeskSpecifics_WindowState_MINIMIZED;
    case ui::WindowShowState::SHOW_STATE_MAXIMIZED:
      return WindowState::WorkspaceDeskSpecifics_WindowState_MAXIMIZED;
    case ui::WindowShowState::SHOW_STATE_FULLSCREEN:
      return WindowState::WorkspaceDeskSpecifics_WindowState_FULLSCREEN;
      // Leave out the default case to let compiler to ensure we have
      // exhaustively handled all cases.
  }
}

// Fill |out_browser_app_window| with the given GURLs as BrowserAppTabs.
void FillBrowserAppTabs(BrowserAppWindow* out_browser_app_window,
                        const std::vector<GURL>& gurls) {
  for (const auto& gurl : gurls) {
    const std::string& url = gurl.spec();
    if (url.empty()) {
      // Skip invalid URLs.
      continue;
    }
    BrowserAppTab* browser_app_tab = out_browser_app_window->add_tabs();
    browser_app_tab->set_url(url);
  }
}

// Fill |out_browser_app_window| with urls and tab information from
// |app_restore_data|.
void FillBrowserAppWindow(BrowserAppWindow* out_browser_app_window,
                          const app_restore::AppRestoreData* app_restore_data) {
  if (app_restore_data->urls.has_value())
    FillBrowserAppTabs(out_browser_app_window, app_restore_data->urls.value());

  if (app_restore_data->active_tab_index.has_value()) {
    out_browser_app_window->set_active_tab_index(
        app_restore_data->active_tab_index.value());
  }
}

// Fill |out_window_bound| with information from |bound|.
void FillWindowBound(WindowBound* out_window_bound, const gfx::Rect& bound) {
  out_window_bound->set_left(bound.x());
  out_window_bound->set_top(bound.y());
  out_window_bound->set_width(bound.width());
  out_window_bound->set_height(bound.height());
}

// Fill |out_app| with information from |window_info|.
void FillAppWithWindowInfo(WorkspaceDeskSpecifics_App* out_app,
                           const app_restore::WindowInfo* window_info) {
  if (window_info->activation_index.has_value())
    out_app->set_z_index(window_info->activation_index.value());

  if (window_info->current_bounds.has_value()) {
    FillWindowBound(out_app->mutable_window_bound(),
                    window_info->current_bounds.value());
  }

  if (window_info->window_state_type.has_value()) {
    out_app->set_window_state(
        FromChromeOsWindowState(window_info->window_state_type.value()));
  }

  if (window_info->pre_minimized_show_state_type.has_value()) {
    out_app->set_pre_minimized_window_state(
        FromUiWindowState(window_info->pre_minimized_show_state_type.value()));
  }

  // AppRestoreData.GetWindowInfo does not include |display_id| in the returned
  // WindowInfo. Therefore, we are not filling |display_id| here.
}

//  Fill |out_app| with |display_id| from |app_restore_data|.
void FillAppWithDisplayId(WorkspaceDeskSpecifics_App* out_app,
                          const app_restore::AppRestoreData* app_restore_data) {
  if (app_restore_data->display_id.has_value())
    out_app->set_display_id(app_restore_data->display_id.value());
}

// Fill |out_app| with |app_restore_data|.
void FillApp(WorkspaceDeskSpecifics_App* out_app,
             const std::string& app_id,
             const apps::mojom::AppType app_type,
             const app_restore::AppRestoreData* app_restore_data) {
  FillAppWithWindowInfo(out_app, app_restore_data->GetWindowInfo().get());

  // AppRestoreData.GetWindowInfo does not include |display_id| in the returned
  // WindowInfo. We need to fill the |display_id| from AppRestoreData.
  FillAppWithDisplayId(out_app, app_restore_data);

  // See definition components/services/app_service/public/mojom/types.mojom
  switch (app_type) {
    case apps::mojom::AppType::kWeb: {
      if (extension_misc::kChromeAppId == app_id) {
        // Chrome Browser Window.
        BrowserAppWindow* browser_app_window =
            out_app->mutable_app()->mutable_browser_app_window();
        FillBrowserAppWindow(browser_app_window, app_restore_data);
      } else {
        // PWA app.
        ProgressiveWebApp* pwa_window =
            out_app->mutable_app()->mutable_progress_web_app();
        pwa_window->set_app_id(app_id);
        if (app_restore_data->title.has_value()) {
          pwa_window->set_title(
              base::UTF16ToUTF8(app_restore_data->title.value()));
        }
      }
      break;
    }
    case apps::mojom::AppType::kStandaloneBrowser: {
      // Lacros Browser App. This is currently unsupported.
      // Note, Lacros-chrome has app ID kLacrosAppId, that is different than
      // kChromeAppId.
      break;
    }
    case apps::mojom::AppType::kExtension: {
      // Chrome extension backed app, Chrome Apps
      ChromeApp* chrome_app_window =
          out_app->mutable_app()->mutable_chrome_app();
      chrome_app_window->set_app_id(app_id);
      if (app_restore_data->title.has_value()) {
        chrome_app_window->set_title(
            base::UTF16ToUTF8(app_restore_data->title.value()));
      }
      break;
    }
    default: {
      // Unhandled app type.
      break;
    }
  }
}

// Fill |out_window_info| with information from Sync proto |app|.
void FillWindowInfoFromProto(app_restore::WindowInfo* out_window_info,
                             sync_pb::WorkspaceDeskSpecifics_App& app) {
  if (app.has_window_state() &&
      sync_pb::WorkspaceDeskSpecifics_WindowState_IsValid(app.window_state())) {
    out_window_info->window_state_type.emplace(
        ToChromeOsWindowState(app.window_state()));
  }

  if (app.has_window_bound()) {
    out_window_info->current_bounds.emplace(
        app.window_bound().left(), app.window_bound().top(),
        app.window_bound().width(), app.window_bound().height());
  }

  if (app.has_z_index())
    out_window_info->activation_index.emplace(app.z_index());

  if (app.has_display_id())
    out_window_info->display_id.emplace(app.display_id());

  if (app.has_pre_minimized_window_state() &&
      sync_pb::WorkspaceDeskSpecifics_WindowState_IsValid(app.window_state())) {
    out_window_info->pre_minimized_show_state_type.emplace(
        ToUiWindowState(app.pre_minimized_window_state()));
  }
}

// Convert a desk template to |app_restore::RestoreData|.
std::unique_ptr<app_restore::RestoreData> ConvertToRestoreData(
    const sync_pb::WorkspaceDeskSpecifics& entry_proto) {
  std::unique_ptr<app_restore::RestoreData> restore_data =
      std::make_unique<app_restore::RestoreData>();

  for (auto app_proto : entry_proto.desk().apps()) {
    std::unique_ptr<app_restore::AppLaunchInfo> app_launch_info =
        ConvertToAppLaunchInfo(app_proto);
    if (!app_launch_info) {
      // Skip unsupported app.
      continue;
    }

    const std::string app_id = app_launch_info->app_id;
    restore_data->AddAppLaunchInfo(std::move(app_launch_info));

    app_restore::WindowInfo app_window_info;
    FillWindowInfoFromProto(&app_window_info, app_proto);

    restore_data->ModifyWindowInfo(app_id, app_proto.window_id(),
                                   app_window_info);
  }

  return restore_data;
}

// Fill a desk template |out_entry_proto| with information from
// |restore_data|.
void FillWorkspaceDeskSpecifics(
    sync_pb::WorkspaceDeskSpecifics* out_entry_proto,
    apps::AppRegistryCache* apps_cache,
    const app_restore::RestoreData* restore_data) {
  DCHECK(apps_cache);

  for (auto const& app_id_to_launch_list :
       restore_data->app_id_to_launch_list()) {
    const std::string app_id = app_id_to_launch_list.first;

    for (auto const& window_id_to_launch_info : app_id_to_launch_list.second) {
      const int window_id = window_id_to_launch_info.first;
      const app_restore::AppRestoreData* app_restore_data =
          window_id_to_launch_info.second.get();
      // The apps cache returns kExtension for browser windows, therefore we
      // short circuit the cache retrieval if we get the browser ID.
      const apps::mojom::AppType app_type =
          app_id == extension_misc::kChromeAppId
              ? apps::mojom::AppType::kWeb
              : apps_cache->GetAppType(app_id);

      WorkspaceDeskSpecifics_App* app =
          out_entry_proto->mutable_desk()->add_apps();
      app->set_window_id(window_id);
      FillApp(app, app_id, app_type, app_restore_data);
    }
  }
}

}  // namespace

DeskSyncBridge::DeskSyncBridge(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    syncer::OnceModelTypeStoreFactory create_store_callback,
    const AccountId& account_id)
    : ModelTypeSyncBridge(std::move(change_processor)),
      is_ready_(false),
      account_id_(account_id) {
  std::move(create_store_callback)
      .Run(syncer::WORKSPACE_DESK,
           base::BindOnce(&DeskSyncBridge::OnStoreCreated,
                          weak_ptr_factory_.GetWeakPtr()));
}

DeskSyncBridge::~DeskSyncBridge() = default;

std::unique_ptr<DeskTemplate> DeskSyncBridge::FromSyncProto(
    const sync_pb::WorkspaceDeskSpecifics& pb_entry) {
  const std::string uuid(pb_entry.uuid());
  if (uuid.empty() || !base::GUID::ParseCaseInsensitive(uuid).is_valid())
    return nullptr;

  const base::Time created_time = desk_template_conversion::ProtoTimeToTime(
      pb_entry.created_time_windows_epoch_micros());

  // Protobuf parsing enforces UTF-8 encoding for all strings.
  std::unique_ptr<DeskTemplate> desk_template = std::make_unique<DeskTemplate>(
      uuid, ash::DeskTemplateSource::kUser, pb_entry.name(), created_time);

  if (pb_entry.has_updated_time_windows_epoch_micros()) {
    desk_template->set_updated_time(desk_template_conversion::ProtoTimeToTime(
        pb_entry.updated_time_windows_epoch_micros()));
  }

  desk_template->set_desk_restore_data(ConvertToRestoreData(pb_entry));
  return desk_template;
}

std::unique_ptr<syncer::MetadataChangeList>
DeskSyncBridge::CreateMetadataChangeList() {
  return ModelTypeStore::WriteBatch::CreateMetadataChangeList();
}

absl::optional<syncer::ModelError> DeskSyncBridge::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  // MergeSyncData will be called when Desk Template model type is enabled to
  // start syncing. There could be local desk templates that user has created
  // before enabling sync or during the time when Desk Template sync is
  // disabled. We should merge local and server data. We will send all
  // local-only templates to server and save server templates to local.

  UploadLocalOnlyData(metadata_change_list.get(), entity_data);

  // Apply server changes locally. Currently, if a template exists on both
  // local and server side, the server version will win.
  // TODO(yzd) We will add a template update timestamp and update this logic to
  // be: for templates that exist on both local and server side, we will keep
  // the one with later update timestamp.
  return ApplySyncChanges(std::move(metadata_change_list),
                          std::move(entity_data));
}

absl::optional<syncer::ModelError> DeskSyncBridge::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  std::vector<const DeskTemplate*> added_or_updated;
  std::vector<std::string> removed;
  std::unique_ptr<ModelTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();

  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    const base::GUID uuid =
        base::GUID::ParseCaseInsensitive(change->storage_key());
    if (!uuid.is_valid()) {
      // Skip invalid storage keys.
      continue;
    }

    switch (change->type()) {
      case syncer::EntityChange::ACTION_DELETE: {
        if (entries_.find(uuid) != entries_.end()) {
          entries_.erase(uuid);
          batch->DeleteData(uuid.AsLowercaseString());
          removed.push_back(uuid.AsLowercaseString());
        }
        break;
      }
      case syncer::EntityChange::ACTION_UPDATE:
      case syncer::EntityChange::ACTION_ADD: {
        const sync_pb::WorkspaceDeskSpecifics& specifics =
            change->data().specifics.workspace_desk();

        std::unique_ptr<DeskTemplate> remote_entry =
            DeskSyncBridge::FromSyncProto(specifics);
        if (!remote_entry) {
          // Skip invalid entries.
          continue;
        }

        DCHECK_EQ(uuid, remote_entry->uuid());
        std::string serialized_remote_entry = specifics.SerializeAsString();

        // Add/update the remote_entry to the model.
        entries_[uuid] = std::move(remote_entry);
        added_or_updated.push_back(GetEntryByUUID(uuid));

        // Write to the store.
        batch->WriteData(uuid.AsLowercaseString(), serialized_remote_entry);
        break;
      }
    }
  }

  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  Commit(std::move(batch));

  NotifyRemoteDeskTemplateAddedOrUpdated(added_or_updated);
  NotifyRemoteDeskTemplateDeleted(removed);

  return absl::nullopt;
}

void DeskSyncBridge::GetData(StorageKeyList storage_keys,
                             DataCallback callback) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();

  for (const std::string& uuid : storage_keys) {
    const DeskTemplate* entry =
        GetEntryByUUID(base::GUID::ParseCaseInsensitive(uuid));
    if (!entry) {
      continue;
    }

    batch->Put(uuid, CopyToEntityData(ToSyncProto(entry)));
  }
  std::move(callback).Run(std::move(batch));
}

void DeskSyncBridge::GetAllDataForDebugging(DataCallback callback) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& it : entries_) {
    batch->Put(it.first.AsLowercaseString(),
               CopyToEntityData(ToSyncProto(it.second.get())));
  }
  std::move(callback).Run(std::move(batch));
}

std::string DeskSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

std::string DeskSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  return entity_data.specifics.workspace_desk().uuid();
}

void DeskSyncBridge::GetAllEntries(GetAllEntriesCallback callback) {
  std::vector<DeskTemplate*> entries;

  if (!IsReady()) {
    std::move(callback).Run(GetAllEntriesStatus::kFailure, std::move(entries));
    return;
  }

  for (const auto& it : policy_entries_)
    entries.push_back(it.get());

  for (const auto& it : entries_) {
    DCHECK_EQ(it.first, it.second->uuid());
    entries.push_back(it.second.get());
  }

  std::move(callback).Run(GetAllEntriesStatus::kOk, std::move(entries));
}

void DeskSyncBridge::GetEntryByUUID(const std::string& uuid_str,
                                    GetEntryByUuidCallback callback) {
  if (!IsReady()) {
    std::move(callback).Run(GetEntryByUuidStatus::kFailure,
                            std::unique_ptr<DeskTemplate>());
    return;
  }

  const base::GUID uuid = base::GUID::ParseCaseInsensitive(uuid_str);
  if (!uuid.is_valid()) {
    std::move(callback).Run(GetEntryByUuidStatus::kInvalidUuid,
                            std::unique_ptr<DeskTemplate>());
    return;
  }

  auto it = entries_.find(uuid);
  if (it == entries_.end()) {
    std::move(callback).Run(GetEntryByUuidStatus::kNotFound,
                            std::unique_ptr<DeskTemplate>());
  } else {
    std::move(callback).Run(GetEntryByUuidStatus::kOk,
                            it->second.get()->Clone());
  }
}

void DeskSyncBridge::AddOrUpdateEntry(std::unique_ptr<DeskTemplate> new_entry,
                                      AddOrUpdateEntryCallback callback) {
  if (!IsReady()) {
    // This sync bridge has not finished initializing. Do not save the new entry
    // yet.
    std::move(callback).Run(AddOrUpdateEntryStatus::kFailure);
    return;
  }

  base::GUID uuid = new_entry->uuid();
  if (!uuid.is_valid()) {
    std::move(callback).Run(AddOrUpdateEntryStatus::kInvalidArgument);
    return;
  }

  // When a user creates a desk template locally, the desk template has |kUser|
  // as its source. Only user desk templates should be saved to Sync.
  DCHECK_EQ(DeskTemplateSource::kUser, new_entry->source());

  auto entry = new_entry->Clone();
  entry->set_template_name(
      base::CollapseWhitespace(new_entry->template_name(), true));

  std::unique_ptr<ModelTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  // Add/update this entry to the store and model.
  auto entity_data = CopyToEntityData(ToSyncProto(entry.get()));

  change_processor()->Put(uuid.AsLowercaseString(), std::move(entity_data),
                          batch->GetMetadataChangeList());

  entries_[uuid] = std::move(entry);
  const DeskTemplate* result = GetEntryByUUID(uuid);

  batch->WriteData(uuid.AsLowercaseString(),
                   ToSyncProto(result).SerializeAsString());

  Commit(std::move(batch));

  std::move(callback).Run(AddOrUpdateEntryStatus::kOk);
}

void DeskSyncBridge::DeleteEntry(const std::string& uuid_str,
                                 DeleteEntryCallback callback) {
  if (!IsReady()) {
    // This sync bridge has not finished initializing.
    // Cannot delete anything.
    std::move(callback).Run(DeleteEntryStatus::kFailure);
    return;
  }

  const base::GUID uuid = base::GUID::ParseCaseInsensitive(uuid_str);

  if (GetEntryByUUID(uuid) == nullptr) {
    // Consider the deletion successful if the entry does not exist.
    std::move(callback).Run(DeleteEntryStatus::kOk);
    return;
  }

  std::unique_ptr<ModelTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();

  change_processor()->Delete(uuid.AsLowercaseString(),
                             batch->GetMetadataChangeList());

  entries_.erase(uuid);

  batch->DeleteData(uuid.AsLowercaseString());

  Commit(std::move(batch));

  std::move(callback).Run(DeleteEntryStatus::kOk);
}

void DeskSyncBridge::DeleteAllEntries(DeleteEntryCallback callback) {
  if (!IsReady()) {
    // This sync bridge has not finished initializing.
    // Cannot delete anything.
    std::move(callback).Run(DeleteEntryStatus::kFailure);
    return;
  }

  std::unique_ptr<ModelTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();

  std::vector<base::GUID> all_uuids = GetAllEntryUuids();

  for (const auto& uuid : all_uuids) {
    change_processor()->Delete(uuid.AsLowercaseString(),
                               batch->GetMetadataChangeList());
    batch->DeleteData(uuid.AsLowercaseString());
  }
  entries_.clear();

  std::move(callback).Run(DeleteEntryStatus::kOk);
}

std::size_t DeskSyncBridge::GetEntryCount() const {
  return entries_.size();
}

std::size_t DeskSyncBridge::GetMaxEntryCount() const {
  return kMaxTemplateCount;
}

std::vector<base::GUID> DeskSyncBridge::GetAllEntryUuids() const {
  std::vector<base::GUID> keys;

  for (const auto& it : policy_entries_)
    keys.push_back(it.get()->uuid());

  for (const auto& it : entries_) {
    DCHECK_EQ(it.first, it.second->uuid());
    keys.emplace_back(it.first);
  }
  return keys;
}

bool DeskSyncBridge::IsReady() const {
  if (is_ready_) {
    DCHECK(store_);
  }
  return is_ready_;
}

bool DeskSyncBridge::IsSyncing() const {
  return change_processor()->IsTrackingMetadata();
}

sync_pb::WorkspaceDeskSpecifics DeskSyncBridge::ToSyncProto(
    const DeskTemplate* desk_template) {
  apps::AppRegistryCache* cache =
      apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(account_id_);
  DCHECK(cache);

  sync_pb::WorkspaceDeskSpecifics pb_entry;

  pb_entry.set_uuid(desk_template->uuid().AsLowercaseString());
  pb_entry.set_name(base::UTF16ToUTF8(desk_template->template_name()));
  pb_entry.set_created_time_windows_epoch_micros(
      desk_template_conversion::TimeToProtoTime(desk_template->created_time()));
  if (desk_template->WasUpdatedSinceCreation()) {
    pb_entry.set_updated_time_windows_epoch_micros(
        desk_template_conversion::TimeToProtoTime(
            desk_template->GetLastUpdatedTime()));
  }

  if (desk_template->desk_restore_data()) {
    FillWorkspaceDeskSpecifics(&pb_entry, cache,
                               desk_template->desk_restore_data());
  }
  return pb_entry;
}

const DeskTemplate* DeskSyncBridge::GetEntryByUUID(
    const base::GUID& uuid) const {
  auto it = entries_.find(uuid);
  if (it == entries_.end())
    return nullptr;
  return it->second.get();
}

void DeskSyncBridge::NotifyDeskModelLoaded() {
  for (DeskModelObserver& observer : observers_) {
    observer.DeskModelLoaded();
  }
}

void DeskSyncBridge::NotifyRemoteDeskTemplateAddedOrUpdated(
    const std::vector<const DeskTemplate*>& new_entries) {
  if (new_entries.empty()) {
    return;
  }

  for (DeskModelObserver& observer : observers_) {
    observer.EntriesAddedOrUpdatedRemotely(new_entries);
  }
}

void DeskSyncBridge::NotifyRemoteDeskTemplateDeleted(
    const std::vector<std::string>& uuids) {
  if (uuids.empty()) {
    return;
  }

  for (DeskModelObserver& observer : observers_) {
    observer.EntriesRemovedRemotely(uuids);
  }
}

void DeskSyncBridge::OnStoreCreated(
    const absl::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore> store) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  auto stored_desk_templates = std::make_unique<DeskEntries>();
  DeskEntries* stored_desk_templates_copy = stored_desk_templates.get();

  store_ = std::move(store);
  store_->ReadAllDataAndPreprocess(
      base::BindOnce(&ParseDeskTemplatesOnBackendSequence,
                     base::Unretained(stored_desk_templates_copy)),
      base::BindOnce(&DeskSyncBridge::OnReadAllData,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(stored_desk_templates)));
}

void DeskSyncBridge::OnReadAllData(
    std::unique_ptr<DeskEntries> stored_desk_templates,
    const absl::optional<syncer::ModelError>& error) {
  DCHECK(stored_desk_templates);

  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  entries_ = std::move(*stored_desk_templates);

  store_->ReadAllMetadata(base::BindOnce(&DeskSyncBridge::OnReadAllMetadata,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void DeskSyncBridge::OnReadAllMetadata(
    const absl::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  change_processor()->ModelReadyToSync(std::move(metadata_batch));
  is_ready_ = true;
  NotifyDeskModelLoaded();
}

void DeskSyncBridge::OnCommit(const absl::optional<syncer::ModelError>& error) {
  if (error) {
    change_processor()->ReportError(*error);
  }
}

void DeskSyncBridge::Commit(std::unique_ptr<ModelTypeStore::WriteBatch> batch) {
  store_->CommitWriteBatch(std::move(batch),
                           base::BindOnce(&DeskSyncBridge::OnCommit,
                                          weak_ptr_factory_.GetWeakPtr()));
}

void DeskSyncBridge::UploadLocalOnlyData(
    syncer::MetadataChangeList* metadata_change_list,
    const syncer::EntityChangeList& entity_data) {
  std::set<base::GUID> local_keys_to_upload;
  for (const auto& it : entries_) {
    DCHECK_EQ(DeskTemplateSource::kUser, it.second->source());
    local_keys_to_upload.insert(it.first);
  }

  // Strip |local_keys_to_upload| of any key (UUID) that is already known to the
  // server.
  for (const std::unique_ptr<syncer::EntityChange>& change : entity_data) {
    local_keys_to_upload.erase(
        base::GUID::ParseCaseInsensitive(change->storage_key()));
  }

  // Upload the local-only templates.
  for (const base::GUID& uuid : local_keys_to_upload) {
    change_processor()->Put(uuid.AsLowercaseString(),
                            CopyToEntityData(ToSyncProto(entries_[uuid].get())),
                            metadata_change_list);
  }
}

}  // namespace desks_storage
