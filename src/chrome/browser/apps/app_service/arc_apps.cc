// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/arc_apps.h"

#include <algorithm>
#include <utility>

#include "ash/public/cpp/app_menu_constants.h"
#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/arc_apps_factory.h"
#include "chrome/browser/apps/app_service/dip_px_util.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/component_extension_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/services/app_service/public/cpp/intent_filter_util.h"
#include "components/arc/app_permissions/arc_app_permissions_bridge.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/mojom/app_permissions.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "content/public/browser/system_connector.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"

// TODO(crbug.com/826982): consider that, per khmel@, "App icon can be
// overwritten (setTaskDescription) or by assigning the icon for the app
// window. In this case some consumers (Shelf for example) switch to
// overwritten icon... IIRC this applies to shelf items and ArcAppWindow icon".

namespace {

void OnArcAppIconCompletelyLoaded(
    apps::mojom::IconCompression icon_compression,
    int32_t size_hint_in_dip,
    apps::IconEffects icon_effects,
    apps::mojom::Publisher::LoadIconCallback callback,
    ArcAppIcon* icon) {
  if (!icon) {
    std::move(callback).Run(apps::mojom::IconValue::New());
    return;
  }

  apps::mojom::IconValuePtr iv = apps::mojom::IconValue::New();
  iv->icon_compression = icon_compression;
  iv->is_placeholder_icon = false;

  if (icon_compression == apps::mojom::IconCompression::kUncompressed) {
    iv->uncompressed = icon->image_skia();
    if (icon_effects != apps::IconEffects::kNone) {
      apps::ApplyIconEffects(icon_effects, size_hint_in_dip, &iv->uncompressed);
    }
  } else {
    auto& compressed_images = icon->compressed_images();
    auto iter =
        compressed_images.find(apps_util::GetPrimaryDisplayUIScaleFactor());
    if (iter == compressed_images.end()) {
      std::move(callback).Run(apps::mojom::IconValue::New());
      return;
    }
    const std::string& data = iter->second;
    iv->compressed = std::vector<uint8_t>(data.begin(), data.end());
    if (icon_effects != apps::IconEffects::kNone) {
      // TODO(crbug.com/988321): decompress the image, apply icon effects then
      // re-compress.
    }
  }

  std::move(callback).Run(std::move(iv));
}

void UpdateAppPermissions(
    const base::flat_map<arc::mojom::AppPermission,
                         arc::mojom::PermissionStatePtr>& new_permissions,
    std::vector<apps::mojom::PermissionPtr>* permissions) {
  for (const auto& new_permission : new_permissions) {
    auto permission = apps::mojom::Permission::New();
    permission->permission_id = static_cast<uint32_t>(new_permission.first);
    permission->value_type = apps::mojom::PermissionValueType::kBool;
    permission->value = static_cast<uint32_t>(new_permission.second->granted);
    permission->is_managed = new_permission.second->managed;

    permissions->push_back(std::move(permission));
  }
}

base::Optional<arc::UserInteractionType> GetUserInterationType(
    apps::mojom::LaunchSource launch_source) {
  auto user_interaction_type = arc::UserInteractionType::NOT_USER_INITIATED;
  switch (launch_source) {
    // kUnknown is not set anywhere, this case is not valid.
    case apps::mojom::LaunchSource::kUnknown:
      return base::nullopt;
    case apps::mojom::LaunchSource::kFromAppListGrid:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER;
      break;
    case apps::mojom::LaunchSource::kFromAppListGridContextMenu:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER_CONTEXT_MENU;
      break;
    case apps::mojom::LaunchSource::kFromAppListQuery:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER_SEARCH;
      break;
    case apps::mojom::LaunchSource::kFromAppListQueryContextMenu:
      user_interaction_type = arc::UserInteractionType::
          APP_STARTED_FROM_LAUNCHER_SEARCH_CONTEXT_MENU;
      break;
    case apps::mojom::LaunchSource::kFromAppListRecommendation:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER_SUGGESTED_APP;
      break;
    case apps::mojom::LaunchSource::kFromParentalControls:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_SETTINGS;
      break;
    case apps::mojom::LaunchSource::kFromShelf:
      user_interaction_type = arc::UserInteractionType::APP_STARTED_FROM_SHELF;
      break;
    case apps::mojom::LaunchSource::kFromFileManager:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_FILE_MANAGER;
      break;
    case apps::mojom::LaunchSource::kFromLink:
      user_interaction_type = arc::UserInteractionType::APP_STARTED_FROM_LINK;
      break;
    case apps::mojom::LaunchSource::kFromOmnibox:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_OMNIBOX;
      break;
    default:
      NOTREACHED();
      return base::nullopt;
  }
  return user_interaction_type;
}

arc::mojom::IntentInfoPtr CreateArcViewIntent(apps::mojom::IntentPtr intent) {
  arc::mojom::IntentInfoPtr arc_intent;
  if (!intent->scheme.has_value() || !intent->host.has_value() ||
      !intent->path.has_value()) {
    return arc_intent;
  }

  arc_intent = arc::mojom::IntentInfo::New();
  auto uri_components = arc::mojom::UriComponents::New();
  constexpr char kAndroidIntentActionView[] = "android.intent.action.VIEW";
  uri_components->scheme = intent->scheme.value();
  uri_components->authority = intent->host.value();
  uri_components->path = intent->path.value();
  arc_intent->action = kAndroidIntentActionView;
  arc_intent->uri_components = std::move(uri_components);
  return arc_intent;
}

apps::mojom::IntentFilterPtr ConvertArcIntentFilter(
    const arc::IntentFilter& arc_intent_filter) {
  auto intent_filter = apps::mojom::IntentFilter::New();

  std::vector<apps::mojom::ConditionValuePtr> scheme_condition_values;
  for (auto& scheme : arc_intent_filter.schemes()) {
    scheme_condition_values.push_back(apps_util::MakeConditionValue(
        scheme, apps::mojom::PatternMatchType::kNone));
  }
  if (!scheme_condition_values.empty()) {
    auto scheme_condition =
        apps_util::MakeCondition(apps::mojom::ConditionType::kScheme,
                                 std::move(scheme_condition_values));
    intent_filter->conditions.push_back(std::move(scheme_condition));
  }

  std::vector<apps::mojom::ConditionValuePtr> host_condition_values;
  for (auto& authority : arc_intent_filter.authorities()) {
    host_condition_values.push_back(apps_util::MakeConditionValue(
        authority.host(), apps::mojom::PatternMatchType::kNone));
  }
  if (!host_condition_values.empty()) {
    auto host_condition = apps_util::MakeCondition(
        apps::mojom::ConditionType::kHost, std::move(host_condition_values));
    intent_filter->conditions.push_back(std::move(host_condition));
  }

  std::vector<apps::mojom::ConditionValuePtr> path_condition_values;
  for (auto& path : arc_intent_filter.paths()) {
    apps::mojom::PatternMatchType match_type;
    switch (path.match_type()) {
      case arc::mojom::PatternType::PATTERN_LITERAL:
        match_type = apps::mojom::PatternMatchType::kLiteral;
        break;
      case arc::mojom::PatternType::PATTERN_PREFIX:
        match_type = apps::mojom::PatternMatchType::kPrefix;
        break;
      case arc::mojom::PatternType::PATTERN_SIMPLE_GLOB:
        match_type = apps::mojom::PatternMatchType::kGlob;
        break;
    }
    path_condition_values.push_back(
        apps_util::MakeConditionValue(path.pattern(), match_type));
  }
  if (!path_condition_values.empty()) {
    auto path_condition = apps_util::MakeCondition(
        apps::mojom::ConditionType::kPattern, std::move(path_condition_values));
    intent_filter->conditions.push_back(std::move(path_condition));
  }

  return intent_filter;
}

arc::IntentFilter CreateArcIntentFilter(
    const std::string& package_name,
    const apps::mojom::IntentFilterPtr& intent_filter) {
  std::vector<std::string> schemes;
  std::vector<arc::IntentFilter::AuthorityEntry> authorities;
  std::vector<arc::IntentFilter::PatternMatcher> paths;
  for (auto& condition : intent_filter->conditions) {
    switch (condition->condition_type) {
      case apps::mojom::ConditionType::kScheme:
        for (auto& condition_value : condition->condition_values) {
          schemes.push_back(condition_value->value);
        }
        break;
      case apps::mojom::ConditionType::kHost:
        for (auto& condition_value : condition->condition_values) {
          authorities.push_back(arc::IntentFilter::AuthorityEntry(
              /*host=*/condition_value->value, /*port=*/0));
        }
        break;
      case apps::mojom::ConditionType::kPattern:
        for (auto& condition_value : condition->condition_values) {
          arc::mojom::PatternType match_type;
          switch (condition_value->match_type) {
            case apps::mojom::PatternMatchType::kLiteral:
              match_type = arc::mojom::PatternType::PATTERN_LITERAL;
              break;
            case apps::mojom::PatternMatchType::kPrefix:
              match_type = arc::mojom::PatternType::PATTERN_PREFIX;
              break;
            case apps::mojom::PatternMatchType::kGlob:
              match_type = arc::mojom::PatternType::PATTERN_SIMPLE_GLOB;
              break;
            case apps::mojom::PatternMatchType::kNone:
              NOTREACHED();
              return arc::IntentFilter();
          }
          paths.push_back(arc::IntentFilter::PatternMatcher(
              condition_value->value, match_type));
        }
        break;
    }
  }
  // TODO(crbug.com/853604): Add support for other action and category types.
  return arc::IntentFilter(package_name, std::move(authorities),
                           std::move(paths), std::move(schemes));
}

// Check if this intent filter only contains HTTP and HTTPS schemes.
bool IsHttpOrHttpsIntentFilter(
    const apps::mojom::IntentFilterPtr& intent_filter) {
  for (const auto& condition : intent_filter->conditions) {
    if (condition->condition_type != apps::mojom::ConditionType::kScheme) {
      continue;
    }
    for (const auto& condition_value : condition->condition_values) {
      if (condition_value->value != url::kHttpScheme &&
          condition_value->value != url::kHttpsScheme) {
        return false;
      }
    }
    return true;
  }
  // If there is no scheme |condition_type| found, return false.
  return false;
}

void AddPreferredApp(const std::string& app_id,
                     const apps::mojom::IntentFilterPtr& intent_filter,
                     apps::mojom::IntentPtr intent,
                     arc::ArcServiceManager* arc_service_manager,
                     ArcAppListPrefs* prefs) {
  arc::mojom::IntentHelperInstance* instance = nullptr;
  if (arc_service_manager) {
    instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_service_manager->arc_bridge_service()->intent_helper(),
        AddPreferredApp);
  }
  if (!instance) {
    return;
  }
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);

  // If |app_info| doesn't exist, we are trying to set preferences for a
  // non-ARC app. Set the preferred app as the ARC intent helper package.
  const std::string& package_name =
      app_info ? app_info->package_name
               : arc::ArcIntentHelperBridge::kArcIntentHelperPackageName;

  instance->AddPreferredApp(package_name,
                            CreateArcIntentFilter(package_name, intent_filter),
                            CreateArcViewIntent(std::move(intent)));
}

void ResetVerifiedLinks(
    const apps::mojom::IntentFilterPtr& intent_filter,
    const apps::mojom::ReplacedAppPreferencesPtr& replaced_app_preferences,
    arc::ArcServiceManager* arc_service_manager,
    ArcAppListPrefs* prefs) {
  arc::mojom::IntentHelperInstance* instance = nullptr;
  if (arc_service_manager) {
    instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_service_manager->arc_bridge_service()->intent_helper(),
        ResetVerifiedLinks);
  }
  if (!instance) {
    return;
  }
  std::vector<std::string> package_names;

  // Find the apps that needs to reset verified link domain status in ARC.
  for (auto& entry : replaced_app_preferences->replaced_preference) {
    auto app_info = prefs->GetApp(entry.first);
    if (!app_info) {
      continue;
    }
    for (auto& intent_filter : entry.second) {
      if (IsHttpOrHttpsIntentFilter(intent_filter)) {
        package_names.push_back(app_info->package_name);
        break;
      }
    }
  }
  instance->ResetVerifiedLinks(package_names);
}

bool ShouldShow(const ArcAppListPrefs::AppInfo& app_info) {
  return app_info.show_in_launcher;
}

}  // namespace

namespace apps {

// static
ArcApps* ArcApps::Get(Profile* profile) {
  return ArcAppsFactory::GetForProfile(profile);
}

// static
ArcApps* ArcApps::CreateForTesting(Profile* profile,
                                   apps::AppServiceProxy* proxy) {
  return new ArcApps(profile, proxy);
}

ArcApps::ArcApps(Profile* profile) : ArcApps(profile, nullptr) {}

ArcApps::ArcApps(Profile* profile, apps::AppServiceProxy* proxy)
    : profile_(profile), arc_icon_once_loader_(profile) {
  if (!arc::IsArcAllowedForProfile(profile_) ||
      (arc::ArcServiceManager::Get() == nullptr)) {
    return;
  }

  if (!proxy) {
    proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  }
  mojo::Remote<apps::mojom::AppService>& app_service = proxy->AppService();
  if (!app_service.is_bound()) {
    return;
  }

  // Make some observee-observer connections.
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }
  prefs->AddObserver(this);
  proxy->SetArcIsRegistered();

  auto* intent_helper_bridge =
      arc::ArcIntentHelperBridge::GetForBrowserContext(profile_);
  if (intent_helper_bridge) {
    arc_intent_helper_observer_.Add(intent_helper_bridge);
  }

  // There is no MessageCenterController for unit tests, so observe when the
  // MessageCenterController is created in production code.
  if (ash::ArcNotificationsHostInitializer::Get()) {
    notification_initializer_observer_.Add(
        ash::ArcNotificationsHostInitializer::Get());
  }

  PublisherBase::Initialize(app_service, apps::mojom::AppType::kArc);
}

ArcApps::~ArcApps() = default;

void ArcApps::Shutdown() {
  // Disconnect the observee-observer connections that we made during the
  // constructor.
  //
  // This isn't entirely correct. The object returned by
  // ArcAppListPrefs::Get(some_profile) can vary over the lifetime of that
  // profile. If it changed, we'll try to disconnect from different
  // ArcAppListPrefs-related objects than the ones we connected to, at the time
  // of this object's construction.
  //
  // Even so, this is probably harmless, assuming that calling
  // foo->RemoveObserver(bar) is a no-op (and e.g. does not crash) if bar
  // wasn't observing foo in the first place, and assuming that the dangling
  // observee-observer connection on the old foo's are never followed again.
  //
  // To fix this properly, we would probably need to add something like an
  // OnArcAppListPrefsWillBeDestroyed method to ArcAppListPrefs::Observer, and
  // in this class's implementation of that method, disconnect. Furthermore,
  // when the new ArcAppListPrefs object is created, we'll have to somehow be
  // notified so we can re-connect this object as an observer.
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (prefs) {
    prefs->RemoveObserver(this);
  }
  arc_icon_once_loader_.StopObserving(prefs);

  arc_intent_helper_observer_.RemoveAll();
}

void ArcApps::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  std::vector<apps::mojom::AppPtr> apps;
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (prefs) {
    for (const auto& app_id : prefs->GetAppIds()) {
      std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
          prefs->GetApp(app_id);
      if (app_info) {
        apps.push_back(Convert(prefs, app_id, *app_info));
      }
    }
  }
  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  subscriber->OnApps(std::move(apps));
  subscribers_.Add(std::move(subscriber));
}

void ArcApps::LoadIcon(const std::string& app_id,
                       apps::mojom::IconKeyPtr icon_key,
                       apps::mojom::IconCompression icon_compression,
                       int32_t size_hint_in_dip,
                       bool allow_placeholder_icon,
                       LoadIconCallback callback) {
  if (!icon_key) {
    std::move(callback).Run(apps::mojom::IconValue::New());
    return;
  }
  IconEffects icon_effects = static_cast<IconEffects>(icon_key->icon_effects);

  // Treat the Play Store as a special case, loading an icon defined by a
  // resource instead of asking the Android VM (or the cache of previous
  // responses from the Android VM). Presumably this is for bootstrapping:
  // the Play Store icon (the UI for enabling and installing Android apps)
  // should be showable even before the user has installed their first
  // Android app and before bringing up an Android VM for the first time.
  if (app_id == arc::kPlayStoreAppId) {
    LoadPlayStoreIcon(icon_compression, size_hint_in_dip, icon_effects,
                      std::move(callback));
  } else {
    const ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile_);
    DCHECK(arc_prefs);

    // If the app has been removed, immediately terminate the icon request since
    // it can't possibly succeed.
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
        arc_prefs->GetApp(app_id);
    if (!app_info) {
      std::move(callback).Run(apps::mojom::IconValue::New());
      return;
    }

    arc_icon_once_loader_.LoadIcon(
        app_id, size_hint_in_dip, icon_compression,
        base::BindOnce(&OnArcAppIconCompletelyLoaded, icon_compression,
                       size_hint_in_dip, icon_effects, std::move(callback)));
  }
}

void ArcApps::Launch(const std::string& app_id,
                     int32_t event_flags,
                     apps::mojom::LaunchSource launch_source,
                     int64_t display_id) {
  auto user_interaction_type = GetUserInterationType(launch_source);
  if (!user_interaction_type.has_value()) {
    return;
  }

  arc::LaunchApp(profile_, app_id, event_flags, user_interaction_type.value(),
                 display_id);
}

void ArcApps::LaunchAppWithIntent(const std::string& app_id,
                                  int32_t event_flags,
                                  apps::mojom::IntentPtr intent,
                                  apps::mojom::LaunchSource launch_source,
                                  int64_t display_id) {
  auto user_interaction_type = GetUserInterationType(launch_source);
  if (!user_interaction_type.has_value()) {
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("Arc.UserInteraction",
                            user_interaction_type.value());

  auto* arc_service_manager = arc::ArcServiceManager::Get();
  arc::mojom::IntentHelperInstance* instance = nullptr;
  if (arc_service_manager) {
    instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_service_manager->arc_bridge_service()->intent_helper(),
        HandleIntent);
  }
  if (!instance) {
    return;
  }

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }
  const std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      prefs->GetApp(app_id);
  if (!app_info) {
    LOG(ERROR) << "Launch App failed, could not find app with id " << app_id;
    return;
  }

  arc::mojom::ActivityNamePtr activity = arc::mojom::ActivityName::New();
  activity->package_name = app_info->package_name;
  activity->activity_name = app_info->activity;

  auto arc_intent = CreateArcViewIntent(std::move(intent));

  if (!arc_intent) {
    LOG(ERROR) << "Launch App failed, launch intent is not valid";
    return;
  }

  instance->HandleIntent(std::move(arc_intent), std::move(activity));

  prefs->SetLastLaunchTime(app_id);
}

void ArcApps::SetPermission(const std::string& app_id,
                            apps::mojom::PermissionPtr permission) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }
  const std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      prefs->GetApp(app_id);
  if (!app_info) {
    LOG(ERROR) << "SetPermission failed, could not find app with id " << app_id;
    return;
  }

  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager) {
    LOG(WARNING) << "SetPermission failed, ArcServiceManager not available.";
    return;
  }

  auto permission_type =
      static_cast<arc::mojom::AppPermission>(permission->permission_id);
  if (permission->value) {
    auto* permissions_instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_service_manager->arc_bridge_service()->app_permissions(),
        GrantPermission);
    if (permissions_instance) {
      permissions_instance->GrantPermission(app_info->package_name,
                                            permission_type);
    }
  } else {
    auto* permissions_instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_service_manager->arc_bridge_service()->app_permissions(),
        RevokePermission);
    if (permissions_instance) {
      permissions_instance->RevokePermission(app_info->package_name,
                                             permission_type);
    }
  }
}

void ArcApps::Uninstall(const std::string& app_id,
                        bool clear_site_data,
                        bool report_abuse) {
  arc::UninstallArcApp(app_id, profile_);
}

void ArcApps::PauseApp(const std::string& app_id) {
  if (paused_apps_.MaybeAddApp(app_id)) {
    SetIconEffect(app_id);
  }

  constexpr bool kPaused = true;
  Publish(paused_apps_.GetAppWithPauseStatus(apps::mojom::AppType::kArc, app_id,
                                             kPaused),
          subscribers_);

  CloseTasks(app_id);
}

void ArcApps::UnpauseApps(const std::string& app_id) {
  if (paused_apps_.MaybeRemoveApp(app_id)) {
    SetIconEffect(app_id);
  }

  constexpr bool kPaused = false;
  Publish(paused_apps_.GetAppWithPauseStatus(apps::mojom::AppType::kArc, app_id,
                                             kPaused),
          subscribers_);
}

void ArcApps::GetMenuModel(const std::string& app_id,
                           apps::mojom::MenuType menu_type,
                           int64_t display_id,
                           GetMenuModelCallback callback) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    std::move(callback).Run(apps::mojom::MenuItems::New());
    return;
  }
  const std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      prefs->GetApp(app_id);
  if (!app_info) {
    std::move(callback).Run(apps::mojom::MenuItems::New());
    return;
  }

  apps::mojom::MenuItemsPtr menu_items = apps::mojom::MenuItems::New();

  // Add Open item if the app is not opened and not suspended.
  if (!base::Contains(app_id_to_task_ids_, app_id) && !app_info->suspended) {
    AddCommandItem((menu_type == apps::mojom::MenuType::kAppList)
                       ? ash::LAUNCH_NEW
                       : ash::MENU_OPEN_NEW,
                   IDS_APP_CONTEXT_MENU_ACTIVATE_ARC, &menu_items);
  }

  if (app_info->shortcut) {
    AddCommandItem(ash::UNINSTALL, IDS_APP_LIST_REMOVE_SHORTCUT, &menu_items);
  } else if (app_info->ready && !app_info->sticky) {
    AddCommandItem(ash::UNINSTALL, IDS_APP_LIST_UNINSTALL_ITEM, &menu_items);
  }

  // App Info item.
  if (app_info->ready && ShouldShow(*app_info)) {
    AddCommandItem(ash::SHOW_APP_INFO, IDS_APP_CONTEXT_MENU_SHOW_INFO,
                   &menu_items);
  }

  if (menu_type == apps::mojom::MenuType::kShelf &&
      base::Contains(app_id_to_task_ids_, app_id)) {
    AddCommandItem(ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE, &menu_items);
  }

  BuildMenuForShortcut(app_info->package_name, std::move(menu_items),
                       std::move(callback));
}

void ArcApps::OpenNativeSettings(const std::string& app_id) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }
  const std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      prefs->GetApp(app_id);
  if (!app_info) {
    LOG(ERROR) << "Cannot open native settings for " << app_id
               << ". App is not found.";
    return;
  }
  arc::ShowPackageInfo(app_info->package_name,
                       arc::mojom::ShowPackageInfoPage::MAIN,
                       display::Screen::GetScreen()->GetPrimaryDisplay().id());
}

void ArcApps::OnPreferredAppSet(
    const std::string& app_id,
    apps::mojom::IntentFilterPtr intent_filter,
    apps::mojom::IntentPtr intent,
    apps::mojom::ReplacedAppPreferencesPtr replaced_app_preferences) {
  auto* arc_service_manager = arc::ArcServiceManager::Get();

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }
  AddPreferredApp(app_id, intent_filter, std::move(intent), arc_service_manager,
                  prefs);
  ResetVerifiedLinks(intent_filter, replaced_app_preferences,
                     arc_service_manager, prefs);
}

void ArcApps::OnAppRegistered(const std::string& app_id,
                              const ArcAppListPrefs::AppInfo& app_info) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (prefs) {
    Publish(Convert(prefs, app_id, app_info), subscribers_);
  }
}

void ArcApps::OnAppStatesChanged(const std::string& app_id,
                                 const ArcAppListPrefs::AppInfo& app_info) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }

  Publish(Convert(prefs, app_id, app_info), subscribers_);
}

void ArcApps::OnAppRemoved(const std::string& app_id) {
  paused_apps_.MaybeRemoveApp(app_id);

  if (base::Contains(app_id_to_task_ids_, app_id)) {
    for (int task_id : app_id_to_task_ids_[app_id]) {
      task_id_to_app_id_.erase(task_id);
    }
    app_id_to_task_ids_.erase(app_id);
  }

  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kArc;
  app->app_id = app_id;
  app->readiness = apps::mojom::Readiness::kUninstalledByUser;
  Publish(std::move(app), subscribers_);

  mojo::Remote<apps::mojom::AppService>& app_service =
      apps::AppServiceProxyFactory::GetForProfile(profile_)->AppService();
  if (!app_service.is_bound()) {
    return;
  }
  app_service->RemovePreferredApp(apps::mojom::AppType::kArc, app_id);
}

void ArcApps::OnAppIconUpdated(const std::string& app_id,
                               const ArcAppIconDescriptor& descriptor) {
  static constexpr uint32_t icon_effects = 0;
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kArc;
  app->app_id = app_id;
  app->icon_key = icon_key_factory_.MakeIconKey(icon_effects);
  Publish(std::move(app), subscribers_);
}

void ArcApps::OnAppNameUpdated(const std::string& app_id,
                               const std::string& name) {
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kArc;
  app->app_id = app_id;
  app->name = name;
  Publish(std::move(app), subscribers_);
}

void ArcApps::OnAppLastLaunchTimeUpdated(const std::string& app_id) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
  if (!app_info) {
    return;
  }
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kArc;
  app->app_id = app_id;
  app->last_launch_time = app_info->last_launch_time;
  Publish(std::move(app), subscribers_);
}

void ArcApps::OnPackageInstalled(
    const arc::mojom::ArcPackageInfo& package_info) {
  ConvertAndPublishPackageApps(package_info);
}

void ArcApps::OnPackageModified(
    const arc::mojom::ArcPackageInfo& package_info) {
  static constexpr bool update_icon = false;
  ConvertAndPublishPackageApps(package_info, update_icon);
}

void ArcApps::OnPackageListInitialRefreshed() {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }
  // This method is called when ARC++ finishes booting. Do not update the icon;
  // it should be impossible for the icon to have changed since ARC++ has not
  // been running.
  static constexpr bool update_icon = false;
  for (const auto& app_id : prefs->GetAppIds()) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
    if (app_info) {
      Publish(Convert(prefs, app_id, *app_info, update_icon), subscribers_);
    }
  }
}

void ArcApps::OnTaskCreated(int task_id,
                            const std::string& package_name,
                            const std::string& activity,
                            const std::string& intent) {
  const std::string app_id = ArcAppListPrefs::GetAppId(package_name, activity);
  app_id_to_task_ids_[app_id].insert(task_id);
  task_id_to_app_id_[task_id] = app_id;
}

void ArcApps::OnTaskDestroyed(int task_id) {
  auto it = task_id_to_app_id_.find(task_id);
  if (it == task_id_to_app_id_.end()) {
    return;
  }

  const std::string app_id = it->second;
  task_id_to_app_id_.erase(it);
  DCHECK(base::Contains(app_id_to_task_ids_, app_id));
  app_id_to_task_ids_[app_id].erase(task_id);
  if (app_id_to_task_ids_[app_id].empty()) {
    app_id_to_task_ids_.erase(app_id);
  }
}

void ArcApps::OnIntentFiltersUpdated(
    const base::Optional<std::string>& package_name) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }

  auto GetAppInfoAndPublish = [prefs, this](std::string app_id) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
    if (app_info) {
      Publish(Convert(prefs, app_id, *app_info), subscribers_);
    }
  };

  // If there is no specific package_name, update all apps, otherwise update
  // apps for the package.

  // Note: Cannot combine the two for-loops because the return type of
  // GetAppIds() is std::vector<std::string> and the return type of
  // GetAppsForPackage() is std::unordered_set<std::string>.
  if (package_name == base::nullopt) {
    for (const auto& app_id : prefs->GetAppIds()) {
      GetAppInfoAndPublish(app_id);
    }
  } else {
    for (const auto& app_id : prefs->GetAppsForPackage(package_name.value())) {
      GetAppInfoAndPublish(app_id);
    }
  }
}

void ArcApps::OnPreferredAppsChanged() {
  if (!base::FeatureList::IsEnabled(features::kAppServiceIntentHandling)) {
    return;
  }
  mojo::Remote<apps::mojom::AppService>& app_service =
      apps::AppServiceProxyFactory::GetForProfile(profile_)->AppService();
  if (!app_service.is_bound()) {
    return;
  }

  auto* intent_helper_bridge =
      arc::ArcIntentHelperBridge::GetForBrowserContext(profile_);
  if (!intent_helper_bridge) {
    return;
  }

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }

  const std::vector<arc::IntentFilter>& added_preferred_apps =
      intent_helper_bridge->GetAddedPreferredApps();

  for (auto& added_preferred_app : added_preferred_apps) {
    constexpr bool kFromPublisher = true;
    // TODO(crbug.com/853604): Currently only handles one App ID per package.
    // If need to handle multiple activities per package, will need to
    // update ARC to send through the corresponding activity and ensure this
    // activity matches with the main_activity that stored in app_service.
    // Will add an activity field in the arc::mojom::intent_filter.
    // Also need to make sure this still work with the Chrome set preference
    // because the intent filter uplifted for each package doesn't contain
    // activity info.
    std::string app_id =
        prefs->GetAppIdByPackageName(added_preferred_app.package_name());
    app_service->AddPreferredApp(apps::mojom::AppType::kArc, app_id,
                                 ConvertArcIntentFilter(added_preferred_app),
                                 /*intent=*/nullptr, kFromPublisher);
  }

  const std::vector<arc::IntentFilter>& deleted_preferred_apps =
      intent_helper_bridge->GetDeletedPreferredApps();

  for (auto& deleted_preferred_app : deleted_preferred_apps) {
    // TODO(crbug.com/853604): Currently only handles one App ID per package.
    // If need to handle multiple activities per package, will need to
    // update ARC to send through the corresponding activity and ensure this
    // activity matches with the main_activity that stored in app_service.
    // Will add an activity field in the arc::mojom::intent_filter.
    // Also need to make sure this still work with the Chrome set preference
    // because the intent filter uplifted for each package doesn't contain
    // activity info.
    std::string app_id =
        prefs->GetAppIdByPackageName(deleted_preferred_app.package_name());
    app_service->RemovePreferredAppForFilter(
        apps::mojom::AppType::kArc, app_id,
        ConvertArcIntentFilter(deleted_preferred_app));
  }
}

void ArcApps::OnSetArcNotificationsInstance(
    ash::ArcNotificationManagerBase* arc_notification_manager) {
  DCHECK(arc_notification_manager);
  notification_observer_.Add(arc_notification_manager);
}

void ArcApps::OnArcNotificationInitializerDestroyed(
    ash::ArcNotificationsHostInitializer* initializer) {
  notification_initializer_observer_.Remove(initializer);
}

void ArcApps::OnNotificationUpdated(const std::string& notification_id,
                                    const std::string& app_id) {
  if (app_id.empty()) {
    return;
  }

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }

  const std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      prefs->GetApp(app_id);
  if (!app_info) {
    return;
  }

  app_notifications_.AddNotification(app_id, notification_id);
  Publish(app_notifications_.GetAppWithHasBadgeStatus(
              apps::mojom::AppType::kArc, app_id),
          subscribers_);
}

void ArcApps::OnNotificationRemoved(const std::string& notification_id) {
  const std::string& app_id =
      app_notifications_.GetAppIdForNotification(notification_id);
  if (app_id.empty()) {
    return;
  }

  app_notifications_.RemoveNotification(notification_id);
  Publish(app_notifications_.GetAppWithHasBadgeStatus(
              apps::mojom::AppType::kArc, app_id),
          subscribers_);
}

void ArcApps::OnArcNotificationManagerDestroyed(
    ash::ArcNotificationManagerBase* notification_manager) {
  notification_observer_.Remove(notification_manager);
}

void ArcApps::LoadPlayStoreIcon(apps::mojom::IconCompression icon_compression,
                                int32_t size_hint_in_dip,
                                IconEffects icon_effects,
                                LoadIconCallback callback) {
  // Use overloaded Chrome icon for Play Store that is adapted to Chrome style.
  constexpr bool quantize_to_supported_scale_factor = true;
  int size_hint_in_px = apps_util::ConvertDipToPx(
      size_hint_in_dip, quantize_to_supported_scale_factor);
  int resource_id = (size_hint_in_px <= 32) ? IDR_ARC_SUPPORT_ICON_32
                                            : IDR_ARC_SUPPORT_ICON_192;
  constexpr bool is_placeholder_icon = false;
  LoadIconFromResource(icon_compression, size_hint_in_dip, resource_id,
                       is_placeholder_icon, icon_effects, std::move(callback));
}

apps::mojom::InstallSource GetInstallSource(
    const ArcAppListPrefs* prefs,
    const ArcAppListPrefs::AppInfo* app_info) {
  // Sticky represents apps that cannot be uninstalled and are installed by the
  // system.
  if (app_info->sticky) {
    return apps::mojom::InstallSource::kSystem;
  }

  if (prefs->IsDefault(app_info->package_name)) {
    return apps::mojom::InstallSource::kDefault;
  }

  if (prefs->IsOem(app_info->package_name)) {
    return apps::mojom::InstallSource::kOem;
  }

  if (prefs->IsControlledByPolicy(app_info->package_name)) {
    return apps::mojom::InstallSource::kPolicy;
  }

  return apps::mojom::InstallSource::kUser;
}

apps::mojom::AppPtr ArcApps::Convert(ArcAppListPrefs* prefs,
                                     const std::string& app_id,
                                     const ArcAppListPrefs::AppInfo& app_info,
                                     bool update_icon) {
  apps::mojom::AppPtr app = PublisherBase::MakeApp(
      apps::mojom::AppType::kArc, app_id,
      app_info.suspended ? apps::mojom::Readiness::kDisabledByPolicy
                         : apps::mojom::Readiness::kReady,
      app_info.name, GetInstallSource(prefs, &app_info));

  app->publisher_id = app_info.package_name;

  auto paused = paused_apps_.IsPaused(app_id)
                    ? apps::mojom::OptionalBool::kTrue
                    : apps::mojom::OptionalBool::kFalse;

  if (update_icon) {
    IconEffects icon_effects = IconEffects::kNone;
    if (app_info.suspended) {
      icon_effects =
          static_cast<IconEffects>(icon_effects | IconEffects::kBlocked);
    }
    if (paused == apps::mojom::OptionalBool::kTrue) {
      icon_effects =
          static_cast<IconEffects>(icon_effects | IconEffects::kPaused);
    }
    app->icon_key = icon_key_factory_.MakeIconKey(icon_effects);
  }

  app->last_launch_time = app_info.last_launch_time;
  app->install_time = app_info.install_time;

  auto show = ShouldShow(app_info) ? apps::mojom::OptionalBool::kTrue
                                   : apps::mojom::OptionalBool::kFalse;
  app->show_in_launcher = show;
  app->show_in_search = show;
  app->show_in_management = show;

  app->paused = paused;

  std::unique_ptr<ArcAppListPrefs::PackageInfo> package =
      prefs->GetPackage(app_info.package_name);
  if (package) {
    UpdateAppPermissions(package->permissions, &app->permissions);
  }

  auto* intent_helper_bridge =
      arc::ArcIntentHelperBridge::GetForBrowserContext(profile_);
  if (intent_helper_bridge) {
    UpdateAppIntentFilters(app_info.package_name, intent_helper_bridge,
                           &app->intent_filters);
  }

  return app;
}

void ArcApps::ConvertAndPublishPackageApps(
    const arc::mojom::ArcPackageInfo& package_info,
    bool update_icon) {
  if (!package_info.permissions.has_value()) {
    return;
  }
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (prefs) {
    for (const auto& app_id :
         prefs->GetAppsForPackage(package_info.package_name)) {
      std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
          prefs->GetApp(app_id);
      if (app_info) {
        Publish(Convert(prefs, app_id, *app_info, update_icon), subscribers_);
      }
    }
  }
}

void ArcApps::SetIconEffect(const std::string& app_id) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
  if (!app_info) {
    return;
  }

  IconEffects icon_effects = IconEffects::kNone;
  if (app_info->suspended) {
    icon_effects =
        static_cast<IconEffects>(icon_effects | IconEffects::kBlocked);
  }
  if (paused_apps_.IsPaused(app_id)) {
    icon_effects =
        static_cast<IconEffects>(icon_effects | IconEffects::kPaused);
  }

  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kArc;
  app->app_id = app_id;
  app->icon_key = icon_key_factory_.MakeIconKey(icon_effects);
  Publish(std::move(app), subscribers_);
}

void ArcApps::CloseTasks(const std::string& app_id) {
  if (!base::FeatureList::IsEnabled(features::kAppServiceInstanceRegistry)) {
    return;
  }

  if (!base::Contains(app_id_to_task_ids_, app_id)) {
    return;
  }

  for (int task_id : app_id_to_task_ids_[app_id]) {
    arc::CloseTask(task_id);
    task_id_to_app_id_.erase(task_id);
  }
  app_id_to_task_ids_.erase(app_id);
}

void ArcApps::UpdateAppIntentFilters(
    std::string package_name,
    arc::ArcIntentHelperBridge* intent_helper_bridge,
    std::vector<apps::mojom::IntentFilterPtr>* intent_filters) {
  const std::vector<arc::IntentFilter>& arc_intent_filters =
      intent_helper_bridge->GetIntentFilterForPackage(package_name);
  for (auto& arc_intent_filter : arc_intent_filters) {
    intent_filters->push_back(ConvertArcIntentFilter(arc_intent_filter));
  }
}

void ArcApps::BuildMenuForShortcut(const std::string& package_name,
                                   apps::mojom::MenuItemsPtr menu_items,
                                   GetMenuModelCallback callback) {
  // The previous request is cancelled, and start a new request if the callback
  // of the previous request is not called.
  arc_app_shortcuts_request_ =
      std::make_unique<arc::ArcAppShortcutsRequest>(base::BindOnce(
          &ArcApps::OnGetAppShortcutItems, weak_ptr_factory_.GetWeakPtr(),
          base::TimeTicks::Now(), std::move(menu_items), std::move(callback)));
  arc_app_shortcuts_request_->StartForPackage(package_name);
}

void ArcApps::OnGetAppShortcutItems(
    const base::TimeTicks start_time,
    apps::mojom::MenuItemsPtr menu_items,
    GetMenuModelCallback callback,
    std::unique_ptr<arc::ArcAppShortcutItems> app_shortcut_items) {
  if (!app_shortcut_items || app_shortcut_items->empty()) {
    // No need log time for empty requests.
    std::move(callback).Run(std::move(menu_items));
    arc_app_shortcuts_request_.reset();
    return;
  }

  arc::ArcAppShortcutItems& items = *app_shortcut_items;
  // Sort the shortcuts based on two rules: (1) Static (declared in manifest)
  // shortcuts and then dynamic shortcuts; (2) Within each shortcut type
  // (static and dynamic), shortcuts are sorted in order of increasing rank.
  std::sort(items.begin(), items.end(),
            [](const arc::ArcAppShortcutItem& item1,
               const arc::ArcAppShortcutItem& item2) {
              return std::tie(item1.type, item1.rank) <
                     std::tie(item2.type, item2.rank);
            });

  AddSeparator(ui::DOUBLE_SEPARATOR, &menu_items);
  int command_id = ash::LAUNCH_APP_SHORTCUT_FIRST;
  for (const auto& item : items) {
    if (command_id != ash::LAUNCH_APP_SHORTCUT_FIRST) {
      AddSeparator(ui::PADDED_SEPARATOR, &menu_items);
    }
    AddArcCommandItem(command_id++, item.shortcut_id, item.short_label,
                      item.icon, &menu_items);
  }
  std::move(callback).Run(std::move(menu_items));
  arc_app_shortcuts_request_.reset();

  UMA_HISTOGRAM_TIMES("Arc.AppShortcuts.BuildMenuTime",
                      base::TimeTicks::Now() - start_time);
}

}  // namespace apps
