// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/app_service_mojom_impl.h"

#include <iterator>
#include <utility>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/token.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/preferred_apps_converter.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/clone_traits.h"

namespace {

const base::FilePath::CharType kPreferredAppsDirname[] =
    FILE_PATH_LITERAL("PreferredApps");

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PreferredAppsFileIOAction {
  kWriteSuccess = 0,
  kWriteFailed = 1,
  kReadSuccess = 2,
  kReadFailed = 3,
  kMaxValue = kReadFailed,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PreferredAppsUpdateAction {
  kAdd = 0,
  kDeleteForFilter = 1,
  kDeleteForAppId = 2,
  kUpgraded = 3,
  kMaxValue = kUpgraded,
};

void Connect(apps::mojom::Publisher* publisher,
             apps::mojom::Subscriber* subscriber) {
  mojo::PendingRemote<apps::mojom::Subscriber> clone;
  subscriber->Clone(clone.InitWithNewPipeAndPassReceiver());
  // TODO: replace nullptr with a ConnectOptions.
  publisher->Connect(std::move(clone), nullptr);
}

void LogPreferredAppEntryCount(int entry_count) {
  base::UmaHistogramCounts10000("Apps.PreferredApps.EntryCount", entry_count);
}

// Performs blocking I/O. Called on another thread.
void WriteDataBlocking(const base::FilePath& preferred_apps_file,
                       const std::string& preferred_apps) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  bool write_success =
      base::WriteFile(preferred_apps_file, preferred_apps.c_str(),
                      preferred_apps.size()) != -1;
  if (!write_success) {
    DVLOG(0) << "Fail to write preferred apps to " << preferred_apps_file;
  }
}

// Performs blocking I/O. Called on another thread.
std::string ReadDataBlocking(const base::FilePath& preferred_apps_file) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  std::string preferred_apps_string;
  base::ReadFileToString(preferred_apps_file, &preferred_apps_string);
  return preferred_apps_string;
}

}  // namespace

namespace apps {

AppServiceMojomImpl::AppServiceMojomImpl(
    const base::FilePath& profile_dir,
    base::OnceClosure read_completed_for_testing,
    base::OnceClosure write_completed_for_testing)
    : profile_dir_(profile_dir),
      should_write_preferred_apps_to_file_(false),
      writing_preferred_apps_(false),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      read_completed_for_testing_(std::move(read_completed_for_testing)),
      write_completed_for_testing_(std::move(write_completed_for_testing)) {
  InitializePreferredApps();
}

AppServiceMojomImpl::~AppServiceMojomImpl() = default;

void AppServiceMojomImpl::BindReceiver(
    mojo::PendingReceiver<apps::mojom::AppService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void AppServiceMojomImpl::FlushMojoCallsForTesting() {
  subscribers_.FlushForTesting();
  receivers_.FlushForTesting();
}

void AppServiceMojomImpl::RegisterPublisher(
    mojo::PendingRemote<apps::mojom::Publisher> publisher_remote,
    apps::mojom::AppType app_type) {
  mojo::Remote<apps::mojom::Publisher> publisher(std::move(publisher_remote));
  // Connect the new publisher with every registered subscriber.
  for (auto& subscriber : subscribers_) {
    ::Connect(publisher.get(), subscriber.get());
  }

  // Check that no previous publisher has registered for the same app_type.
  CHECK(publishers_.find(app_type) == publishers_.end());

  // Add the new publisher to the set.
  publisher.set_disconnect_handler(
      base::BindOnce(&AppServiceMojomImpl::OnPublisherDisconnected,
                     base::Unretained(this), app_type));
  auto result = publishers_.emplace(app_type, std::move(publisher));
  CHECK(result.second);
}

void AppServiceMojomImpl::RegisterSubscriber(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  // Connect the new subscriber with every registered publisher.
  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  for (const auto& iter : publishers_) {
    ::Connect(iter.second.get(), subscriber.get());
  }

  // TODO: store the opts somewhere.

  // Initialise the Preferred Apps in the Subscribers on register.
  if (preferred_apps_.IsInitialized()) {
    subscriber->InitializePreferredApps(preferred_apps_.GetValue());
  }

  // Add the new subscriber to the set.
  subscribers_.Add(std::move(subscriber));
}

void AppServiceMojomImpl::LoadIcon(apps::mojom::AppType app_type,
                                   const std::string& app_id,
                                   apps::mojom::IconKeyPtr icon_key,
                                   apps::mojom::IconType icon_type,
                                   int32_t size_hint_in_dip,
                                   bool allow_placeholder_icon,
                                   LoadIconCallback callback) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    std::move(callback).Run(apps::mojom::IconValue::New());
    return;
  }
  iter->second->LoadIcon(app_id, std::move(icon_key), icon_type,
                         size_hint_in_dip, allow_placeholder_icon,
                         std::move(callback));
}

void AppServiceMojomImpl::Launch(apps::mojom::AppType app_type,
                                 const std::string& app_id,
                                 int32_t event_flags,
                                 apps::mojom::LaunchSource launch_source,
                                 apps::mojom::WindowInfoPtr window_info) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->Launch(app_id, event_flags, launch_source,
                       std::move(window_info));
}
void AppServiceMojomImpl::LaunchAppWithFiles(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::FilePathsPtr file_paths) {
  CHECK(file_paths);
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->LaunchAppWithFiles(app_id, event_flags, launch_source,
                                   std::move(file_paths));
}

void AppServiceMojomImpl::LaunchAppWithIntent(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::IntentPtr intent,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::WindowInfoPtr window_info,
    LaunchAppWithIntentCallback callback) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }
  iter->second->LaunchAppWithIntent(app_id, event_flags, std::move(intent),
                                    launch_source, std::move(window_info),
                                    std::move(callback));
}

void AppServiceMojomImpl::SetPermission(apps::mojom::AppType app_type,
                                        const std::string& app_id,
                                        apps::mojom::PermissionPtr permission) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->SetPermission(app_id, std::move(permission));
}

void AppServiceMojomImpl::Uninstall(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    apps::mojom::UninstallSource uninstall_source,
    bool clear_site_data,
    bool report_abuse) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->Uninstall(app_id, uninstall_source, clear_site_data,
                          report_abuse);
}

void AppServiceMojomImpl::PauseApp(apps::mojom::AppType app_type,
                                   const std::string& app_id) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->PauseApp(app_id);
}

void AppServiceMojomImpl::UnpauseApp(apps::mojom::AppType app_type,
                                     const std::string& app_id) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->UnpauseApp(app_id);
}

void AppServiceMojomImpl::StopApp(apps::mojom::AppType app_type,
                                  const std::string& app_id) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->StopApp(app_id);
}

void AppServiceMojomImpl::GetMenuModel(apps::mojom::AppType app_type,
                                       const std::string& app_id,
                                       apps::mojom::MenuType menu_type,
                                       int64_t display_id,
                                       GetMenuModelCallback callback) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    std::move(callback).Run(apps::mojom::MenuItems::New());
    return;
  }

  iter->second->GetMenuModel(app_id, menu_type, display_id,
                             std::move(callback));
}

void AppServiceMojomImpl::ExecuteContextMenuCommand(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    int command_id,
    const std::string& shortcut_id,
    int64_t display_id) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }

  iter->second->ExecuteContextMenuCommand(app_id, command_id, shortcut_id,
                                          display_id);
}

void AppServiceMojomImpl::OpenNativeSettings(apps::mojom::AppType app_type,
                                             const std::string& app_id) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->OpenNativeSettings(app_id);
}

void AppServiceMojomImpl::AddPreferredApp(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    apps::mojom::IntentFilterPtr intent_filter,
    apps::mojom::IntentPtr intent,
    bool from_publisher) {
  // TODO(crbug.com/853604): Make sure the ARC preference init happens after
  // this. Might need to change the interface to call that after read completed.
  // Might also need to record the change before data read and make the update
  // after initialization in the future.
  if (!preferred_apps_.IsInitialized()) {
    DVLOG(0) << "Preferred apps not initialised when try to add.";
    return;
  }

  // TODO(https://crbug.com/853604): Remove this and convert to a DCHECK
  // after finding out the root cause.
  if (app_id.empty()) {
    base::debug::DumpWithoutCrashing();
    return;
  }

  apps::mojom::ReplacedAppPreferencesPtr replaced_apps =
      preferred_apps_.AddPreferredApp(app_id, intent_filter);

  WriteToJSON(profile_dir_, preferred_apps_);

  auto changes = apps::mojom::PreferredAppChanges::New();

  changes->added_filters[app_id].push_back(intent_filter->Clone());
  changes->removed_filters = Clone(replaced_apps->replaced_preference);

  for (auto& subscriber : subscribers_) {
    subscriber->OnPreferredAppsChanged(changes->Clone());
  }

  if (from_publisher || !intent) {
    return;
  }

  // Sync the change to publishers. Because |replaced_app_preference| can
  // be any app type, we should run this for all publishers. Currently
  // only implemented in ARC publisher.
  // TODO(crbug.com/853604): The |replaced_app_preference| can be really big,
  // update this logic to only call the relevant publisher for each app after
  // updating the storage structure.
  for (const auto& iter : publishers_) {
    iter.second->OnPreferredAppSet(app_id, intent_filter->Clone(),
                                   intent->Clone(), replaced_apps->Clone());
  }
}

void AppServiceMojomImpl::RemovePreferredApp(apps::mojom::AppType app_type,
                                             const std::string& app_id) {
  // TODO(crbug.com/853604): Make sure the ARC preference init happens after
  // this. Might need to change the interface to call that after read completed.
  // Might also need to record the change before data read and make the update
  // after initialization in the future.
  if (!preferred_apps_.IsInitialized()) {
    DVLOG(0) << "Preferred apps not initialised when try to remove an app id.";
    return;
  }

  std::vector<apps::mojom::IntentFilterPtr> removed_filters =
      preferred_apps_.DeleteAppId(app_id);
  if (!removed_filters.empty()) {
    WriteToJSON(profile_dir_, preferred_apps_);

    auto changes = apps::mojom::PreferredAppChanges::New();
    changes->removed_filters.emplace(app_id, std::move(removed_filters));
    for (auto& subscriber : subscribers_) {
      subscriber->OnPreferredAppsChanged(changes->Clone());
    }
  }
}

void AppServiceMojomImpl::RemovePreferredAppForFilter(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    apps::mojom::IntentFilterPtr intent_filter) {
  // TODO(crbug.com/853604): Make sure the ARC preference init happens after
  // this. Might need to change the interface to call that after read completed.
  // Might also need to record the change before data read and make the update
  // after initialization in the future.
  if (!preferred_apps_.IsInitialized()) {
    DVLOG(0) << "Preferred apps not initialised when try to remove a filter.";
    return;
  }

  std::vector<apps::mojom::IntentFilterPtr> removed_filters =
      preferred_apps_.DeletePreferredApp(app_id, intent_filter);

  if (!removed_filters.empty()) {
    WriteToJSON(profile_dir_, preferred_apps_);

    auto changes = apps::mojom::PreferredAppChanges::New();
    changes->removed_filters.emplace(app_id, std::move(removed_filters));
    for (auto& subscriber : subscribers_) {
      subscriber->OnPreferredAppsChanged(changes->Clone());
    }
  }
}

void AppServiceMojomImpl::SetSupportedLinksPreference(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    std::vector<apps::mojom::IntentFilterPtr> all_link_filters) {
  if (!preferred_apps_.IsInitialized()) {
    DVLOG(0) << "Preferred apps not initialised when trying to add.";
    return;
  }

  auto changes = apps::mojom::PreferredAppChanges::New();
  auto& added = changes->added_filters;
  auto& removed = changes->removed_filters;

  for (auto& filter : all_link_filters) {
    apps::mojom::ReplacedAppPreferencesPtr replaced_apps =
        preferred_apps_.AddPreferredApp(app_id, filter);
    added[app_id].push_back(std::move(filter));

    // If we removed overlapping supported links when adding the new app, those
    // affected apps no longer handle all their Supported Links filters and so
    // need to have all their other Supported Links filters removed.
    // Additionally, track all removals in the |removed| map so that subscribers
    // can be notified correctly.
    for (auto& replaced_app_and_filters : replaced_apps->replaced_preference) {
      const std::string& removed_app_id = replaced_app_and_filters.first;
      bool first_removal_for_app = !base::Contains(removed, app_id);
      bool did_replace_supported_link = base::ranges::any_of(
          replaced_app_and_filters.second,
          [&removed_app_id](const auto& filter) {
            return apps_util::IsSupportedLinkForApp(removed_app_id, filter);
          });

      std::vector<apps::mojom::IntentFilterPtr>& removed_filters_for_app =
          removed[removed_app_id];
      removed_filters_for_app.insert(
          removed_filters_for_app.end(),
          std::make_move_iterator(replaced_app_and_filters.second.begin()),
          std::make_move_iterator(replaced_app_and_filters.second.end()));

      // We only need to remove other supported links once per app.
      if (first_removal_for_app && did_replace_supported_link) {
        std::vector<apps::mojom::IntentFilterPtr> removed_filters =
            preferred_apps_.DeleteSupportedLinks(removed_app_id);
        removed_filters_for_app.insert(
            removed_filters_for_app.end(),
            std::make_move_iterator(removed_filters.begin()),
            std::make_move_iterator(removed_filters.end()));
      }
    }
  }

  WriteToJSON(profile_dir_, preferred_apps_);

  for (auto& subscriber : subscribers_) {
    subscriber->OnPreferredAppsChanged(changes->Clone());
  }

  // Notify publishers: The new app has been set to open links, and all removed
  // apps no longer handle links.
  const auto publisher_iter = publishers_.find(app_type);
  if (publisher_iter != publishers_.end()) {
    publisher_iter->second->OnSupportedLinksPreferenceChanged(
        app_id, /*open_in_app=*/true);
  }
  for (const auto& removed_app_and_filters : removed) {
    // We don't know what app type the app is, so we have to notify all
    // publishers.
    for (const auto& iter : publishers_) {
      iter.second->OnSupportedLinksPreferenceChanged(
          removed_app_and_filters.first,
          /*open_in_app=*/false);
    }
  }
}

void AppServiceMojomImpl::RemoveSupportedLinksPreference(
    apps::mojom::AppType app_type,
    const std::string& app_id) {
  if (!preferred_apps_.IsInitialized()) {
    DVLOG(0) << "Preferred apps not initialised when try to remove.";
    return;
  }

  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }

  std::vector<apps::mojom::IntentFilterPtr> removed_filters =
      preferred_apps_.DeleteSupportedLinks(app_id);

  if (!removed_filters.empty()) {
    WriteToJSON(profile_dir_, preferred_apps_);

    auto changes = apps::mojom::PreferredAppChanges::New();
    changes->removed_filters.emplace(app_id, std::move(removed_filters));

    for (auto& subscriber : subscribers_) {
      subscriber->OnPreferredAppsChanged(changes->Clone());
    }
  }

  iter->second->OnSupportedLinksPreferenceChanged(app_id,
                                                  /*open_in_app=*/false);
}

void AppServiceMojomImpl::SetResizeLocked(apps::mojom::AppType app_type,
                                          const std::string& app_id,
                                          mojom::OptionalBool locked) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->SetResizeLocked(app_id, locked);
}

void AppServiceMojomImpl::SetWindowMode(apps::mojom::AppType app_type,
                                        const std::string& app_id,
                                        apps::mojom::WindowMode window_mode) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->SetWindowMode(app_id, window_mode);
}

PreferredAppsList& AppServiceMojomImpl::GetPreferredAppsForTesting() {
  return preferred_apps_;
}

void AppServiceMojomImpl::OnPublisherDisconnected(
    apps::mojom::AppType app_type) {
  publishers_.erase(app_type);
}

void AppServiceMojomImpl::InitializePreferredApps() {
  ReadFromJSON(profile_dir_);
}

void AppServiceMojomImpl::WriteToJSON(
    const base::FilePath& profile_dir,
    const apps::PreferredAppsList& preferred_apps) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // If currently is writing preferred apps to file, set a flag to write after
  // the current write completed.
  if (writing_preferred_apps_) {
    should_write_preferred_apps_to_file_ = true;
    return;
  }

  writing_preferred_apps_ = true;

  auto preferred_apps_value =
      apps::ConvertPreferredAppsToValue(preferred_apps.GetReference());

  std::string json_string;
  JSONStringValueSerializer serializer(&json_string);
  serializer.Serialize(preferred_apps_value);

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&WriteDataBlocking,
                     profile_dir.Append(kPreferredAppsDirname), json_string),
      base::BindOnce(&AppServiceMojomImpl::WriteCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AppServiceMojomImpl::WriteCompleted() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  writing_preferred_apps_ = false;
  if (!should_write_preferred_apps_to_file_) {
    // Call the testing callback if it is set.
    if (write_completed_for_testing_) {
      std::move(write_completed_for_testing_).Run();
    }
    return;
  }
  // If need to perform another write, write the most up to date preferred apps
  // from memory to file.
  should_write_preferred_apps_to_file_ = false;
  WriteToJSON(profile_dir_, preferred_apps_);
}

void AppServiceMojomImpl::ReadFromJSON(const base::FilePath& profile_dir) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ReadDataBlocking,
                     profile_dir.Append(kPreferredAppsDirname)),
      base::BindOnce(&AppServiceMojomImpl::ReadCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AppServiceMojomImpl::ReadCompleted(std::string preferred_apps_string) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bool preferred_apps_upgraded = false;
  if (preferred_apps_string.empty()) {
    preferred_apps_.Init();
  } else {
    std::string json_string;
    JSONStringValueDeserializer deserializer(preferred_apps_string);
    int error_code;
    std::string error_message;
    auto preferred_apps_value =
        deserializer.Deserialize(&error_code, &error_message);

    if (!preferred_apps_value) {
      DVLOG(0) << "Fail to deserialize json value from string with error code: "
               << error_code << " and error message: " << error_message;
      preferred_apps_.Init();
    } else {
      preferred_apps_upgraded = IsUpgradedForSharing(*preferred_apps_value);
      auto preferred_apps =
          apps::ParseValueToPreferredApps(*preferred_apps_value);
      if (!preferred_apps_upgraded) {
        UpgradePreferredApps(preferred_apps);
      }
      preferred_apps_.Init(preferred_apps);
    }
  }
  if (!preferred_apps_upgraded) {
    WriteToJSON(profile_dir_, preferred_apps_);
  }

  for (auto& subscriber : subscribers_) {
    subscriber->InitializePreferredApps(preferred_apps_.GetValue());
  }
  if (read_completed_for_testing_) {
    std::move(read_completed_for_testing_).Run();
  }

  LogPreferredAppEntryCount(preferred_apps_.GetEntrySize());
}

}  // namespace apps
