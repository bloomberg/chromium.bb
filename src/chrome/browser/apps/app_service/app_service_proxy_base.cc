// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_proxy_base.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/extend.h"
#include "base/debug/dump_without_crashing.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_source.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "components/services/app_service/app_service_mojom_impl.h"
#include "components/services/app_service/public/cpp/intent_constants.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/services/app_service/public/mojom/types.mojom-forward.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/browser/url_data_source.h"
#include "extensions/common/constants.h"
#include "ui/display/types/display_constants.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/common/chrome_features.h"
#endif

namespace apps {

namespace {

// Utility struct used in GetAppsForIntent.
struct IndexAndGeneric {
  size_t index;
  bool is_generic;
};

std::string GetActivityLabel(const apps::mojom::IntentFilterPtr& filter,
                             const apps::AppUpdate& update) {
  if (filter->activity_label && !filter->activity_label->empty()) {
    return filter->activity_label.value();
  } else {
    return update.Name();
  }
}

}  // anonymous namespace

AppServiceProxyBase::InnerIconLoader::InnerIconLoader(AppServiceProxyBase* host)
    : host_(host), overriding_icon_loader_for_testing_(nullptr) {}

absl::optional<IconKey> AppServiceProxyBase::InnerIconLoader::GetIconKey(
    const std::string& app_id) {
  if (overriding_icon_loader_for_testing_) {
    return overriding_icon_loader_for_testing_->GetIconKey(app_id);
  }

  absl::optional<IconKey> icon_key;
  host_->app_registry_cache_.ForApp(
      app_id,
      [&icon_key](const AppUpdate& update) { icon_key = update.GetIconKey(); });
  return icon_key;
}

std::unique_ptr<IconLoader::Releaser>
AppServiceProxyBase::InnerIconLoader::LoadIconFromIconKey(
    AppType app_type,
    const std::string& app_id,
    const IconKey& icon_key,
    IconType icon_type,
    int32_t size_hint_in_dip,
    bool allow_placeholder_icon,
    apps::LoadIconCallback callback) {
  if (overriding_icon_loader_for_testing_) {
    return overriding_icon_loader_for_testing_->LoadIconFromIconKey(
        app_type, app_id, icon_key, icon_type, size_hint_in_dip,
        allow_placeholder_icon, std::move(callback));
  }

  auto* publisher = host_->GetPublisher(app_type);
  if (!publisher) {
    std::move(callback).Run(std::make_unique<IconValue>());
    return nullptr;
  }

  RecordAppLaunchMetrics(IconLoadingMethod::kViaNonMojomCall);
  publisher->LoadIcon(app_id, icon_key, icon_type, size_hint_in_dip,
                      allow_placeholder_icon, std::move(callback));
  return nullptr;
}

std::unique_ptr<IconLoader::Releaser>
AppServiceProxyBase::InnerIconLoader::LoadIconFromIconKey(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    apps::mojom::IconKeyPtr icon_key,
    apps::mojom::IconType icon_type,
    int32_t size_hint_in_dip,
    bool allow_placeholder_icon,
    apps::mojom::Publisher::LoadIconCallback callback) {
  if (overriding_icon_loader_for_testing_) {
    return overriding_icon_loader_for_testing_->LoadIconFromIconKey(
        app_type, app_id, std::move(icon_key), icon_type, size_hint_in_dip,
        allow_placeholder_icon, std::move(callback));
  }

  if (host_->app_service_.is_connected() && icon_key) {
    // TODO(crbug.com/826982): Mojo doesn't guarantee the order of messages,
    // so multiple calls to this method might not resolve their callbacks in
    // order. As per khmel@, "you may have race here, assume you publish change
    // for the app and app requested new icon. But new icon is not delivered
    // yet and you resolve old one instead. Now new icon arrives asynchronously
    // but you no longer notify the app or do?"
    RecordAppLaunchMetrics(IconLoadingMethod::kViaMojomCall);
    host_->app_service_->LoadIcon(app_type, app_id, std::move(icon_key),
                                  icon_type, size_hint_in_dip,
                                  allow_placeholder_icon, std::move(callback));
  } else {
    std::move(callback).Run(apps::mojom::IconValue::New());
  }
  return nullptr;
}

AppServiceProxyBase::AppServiceProxyBase(Profile* profile)
    : inner_icon_loader_(this),
      icon_coalescer_(&inner_icon_loader_),
      outer_icon_loader_(&icon_coalescer_,
                         apps::IconCache::GarbageCollectionPolicy::kEager),
      profile_(profile) {}

AppServiceProxyBase::~AppServiceProxyBase() = default;

void AppServiceProxyBase::ReInitializeForTesting(Profile* profile) {
  // Some test code creates a profile and profile-linked services, like the App
  // Service, before the profile is fully initialized. Such tests can call this
  // after full profile initialization to ensure the App Service implementation
  // has all of profile state it needs.
  app_service_.reset();
  profile_ = profile;
  is_using_testing_profile_ = true;
  Initialize();
}

bool AppServiceProxyBase::IsValidProfile() {
  if (!profile_) {
    return false;
  }

  // We only initialize the App Service for regular or guest profiles. Non-guest
  // off-the-record profiles do not get an instance.
  if (profile_->IsOffTheRecord() && !profile_->IsGuestSession()) {
    return false;
  }

  return true;
}

void AppServiceProxyBase::Initialize() {
  if (!IsValidProfile()) {
    return;
  }

  browser_app_launcher_ = std::make_unique<apps::BrowserAppLauncher>(profile_);

  app_service_mojom_impl_ =
      std::make_unique<apps::AppServiceMojomImpl>(profile_->GetPath());
  app_service_mojom_impl_->BindReceiver(
      app_service_.BindNewPipeAndPassReceiver());

  if (app_service_.is_connected()) {
    // The AppServiceProxy is a subscriber: something that wants to be able to
    // list all known apps.
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber;
    receivers_.Add(this, subscriber.InitWithNewPipeAndPassReceiver());
    app_service_->RegisterSubscriber(std::move(subscriber), nullptr);
  }

  // Make the chrome://app-icon/ resource available.
  content::URLDataSource::Add(profile_,
                              std::make_unique<apps::AppIconSource>(profile_));
}

AppPublisher* AppServiceProxyBase::GetPublisher(AppType app_type) {
  auto it = publishers_.find(app_type);
  return it == publishers_.end() ? nullptr : it->second;
}

mojo::Remote<apps::mojom::AppService>& AppServiceProxyBase::AppService() {
  return app_service_;
}

apps::AppRegistryCache& AppServiceProxyBase::AppRegistryCache() {
  return app_registry_cache_;
}

apps::AppCapabilityAccessCache&
AppServiceProxyBase::AppCapabilityAccessCache() {
  return app_capability_access_cache_;
}

BrowserAppLauncher* AppServiceProxyBase::BrowserAppLauncher() {
  return browser_app_launcher_.get();
}

apps::PreferredAppsListHandle& AppServiceProxyBase::PreferredApps() {
  return preferred_apps_;
}

void AppServiceProxyBase::RegisterPublisher(AppType app_type,
                                            AppPublisher* publisher) {
  publishers_[app_type] = publisher;
}

absl::optional<IconKey> AppServiceProxyBase::GetIconKey(
    const std::string& app_id) {
  return outer_icon_loader_.GetIconKey(app_id);
}

std::unique_ptr<apps::IconLoader::Releaser>
AppServiceProxyBase::LoadIconFromIconKey(AppType app_type,
                                         const std::string& app_id,
                                         const IconKey& icon_key,
                                         IconType icon_type,
                                         int32_t size_hint_in_dip,
                                         bool allow_placeholder_icon,
                                         apps::LoadIconCallback callback) {
  return outer_icon_loader_.LoadIconFromIconKey(
      app_type, app_id, icon_key, icon_type, size_hint_in_dip,
      allow_placeholder_icon, std::move(callback));
}

std::unique_ptr<apps::IconLoader::Releaser>
AppServiceProxyBase::LoadIconFromIconKey(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    apps::mojom::IconKeyPtr icon_key,
    apps::mojom::IconType icon_type,
    int32_t size_hint_in_dip,
    bool allow_placeholder_icon,
    apps::mojom::Publisher::LoadIconCallback callback) {
  return outer_icon_loader_.LoadIconFromIconKey(
      app_type, app_id, std::move(icon_key), icon_type, size_hint_in_dip,
      allow_placeholder_icon, std::move(callback));
}

void AppServiceProxyBase::Launch(const std::string& app_id,
                                 int32_t event_flags,
                                 apps::mojom::LaunchSource launch_source,
                                 apps::mojom::WindowInfoPtr window_info) {
  if (app_service_.is_connected()) {
    app_registry_cache_.ForOneApp(
        app_id, [this, event_flags, launch_source,
                 &window_info](const apps::AppUpdate& update) {
          if (MaybeShowLaunchPreventionDialog(update)) {
            return;
          }

          RecordAppLaunch(update.AppId(), launch_source);
          RecordAppPlatformMetrics(
              profile_, update, launch_source,
              apps::mojom::LaunchContainer::kLaunchContainerNone);

          app_service_->Launch(update.AppType(), update.AppId(), event_flags,
                               launch_source, std::move(window_info));

          PerformPostLaunchTasks(launch_source);
        });
  }
}

void AppServiceProxyBase::LaunchAppWithFiles(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::FilePathsPtr file_paths) {
  if (app_service_.is_connected()) {
    app_registry_cache_.ForOneApp(
        app_id, [this, event_flags, launch_source,
                 &file_paths](const apps::AppUpdate& update) {
          if (MaybeShowLaunchPreventionDialog(update)) {
            return;
          }

          RecordAppPlatformMetrics(
              profile_, update, launch_source,
              apps::mojom::LaunchContainer::kLaunchContainerNone);

          // TODO(crbug/1117655): File manager records metrics for apps it
          // launched. So we only record launches from other places. We should
          // eventually move those metrics here, after AppService supports all
          // app types launched by file manager.
          if (launch_source != apps::mojom::LaunchSource::kFromFileManager) {
            RecordAppLaunch(update.AppId(), launch_source);
          }

          app_service_->LaunchAppWithFiles(update.AppType(), update.AppId(),
                                           event_flags, launch_source,
                                           std::move(file_paths));

          PerformPostLaunchTasks(launch_source);
        });
  }
}

void AppServiceProxyBase::LaunchAppWithIntent(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::IntentPtr intent,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::WindowInfoPtr window_info,
    apps::mojom::Publisher::LaunchAppWithIntentCallback callback) {
  CHECK(intent);
  if (app_service_.is_connected()) {
    app_registry_cache_.ForOneApp(
        app_id, [this, event_flags, &intent, launch_source, &window_info,
                 callback = std::move(callback)](
                    const apps::AppUpdate& update) mutable {
          if (MaybeShowLaunchPreventionDialog(update)) {
            if (callback)
              std::move(callback).Run(/*success=*/false);
            return;
          }

          // TODO(crbug/1117655): File manager records metrics for apps it
          // launched. So we only record launches from other places. We should
          // eventually move those metrics here, after AppService supports all
          // app types launched by file manager.
          if (launch_source != apps::mojom::LaunchSource::kFromFileManager) {
            RecordAppLaunch(update.AppId(), launch_source);
          }
          RecordAppPlatformMetrics(
              profile_, update, launch_source,
              apps::mojom::LaunchContainer::kLaunchContainerNone);

          app_service_->LaunchAppWithIntent(
              update.AppType(), update.AppId(), event_flags, std::move(intent),
              launch_source, std::move(window_info), std::move(callback));

          PerformPostLaunchTasks(launch_source);
        });
  } else if (callback) {
    std::move(callback).Run(/*success=*/false);
  }
}

void AppServiceProxyBase::LaunchAppWithUrl(
    const std::string& app_id,
    int32_t event_flags,
    GURL url,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::WindowInfoPtr window_info) {
  LaunchAppWithIntent(app_id, event_flags, apps_util::CreateIntentFromUrl(url),
                      launch_source, std::move(window_info));
}

void AppServiceProxyBase::LaunchAppWithParams(AppLaunchParams&& params,
                                              LaunchCallback callback) {
  auto app_type = ConvertMojomAppTypToAppType(
      app_registry_cache_.GetAppType(params.app_id));
  auto* publisher = GetPublisher(app_type);
  if (!publisher) {
    std::move(callback).Run(LaunchResult());
    return;
  }

  app_registry_cache_.ForOneApp(
      params.app_id,
      [this, &params, &callback, &publisher](const apps::AppUpdate& update) {
        if (MaybeShowLaunchPreventionDialog(update)) {
          std::move(callback).Run(LaunchResult());
          return;
        }
        auto launch_source = params.launch_source;
        // TODO(crbug/1117655): File manager records metrics for apps it
        // launched. So we only record launches from other places. We should
        // eventually move those metrics here, after AppService supports all
        // app types launched by file manager.
        if (launch_source != apps::mojom::LaunchSource::kFromFileManager) {
          RecordAppLaunch(update.AppId(), launch_source);
        }

        RecordAppPlatformMetrics(profile_, update, launch_source,
                                 params.container);

        publisher->LaunchAppWithParams(
            std::move(params),
            base::BindOnce(&AppServiceProxyBase::OnLaunched,
                           weak_factory_.GetWeakPtr(), std::move(callback)));

        PerformPostLaunchTasks(launch_source);
      });
}

void AppServiceProxyBase::SetPermission(const std::string& app_id,
                                        apps::mojom::PermissionPtr permission) {
  if (app_service_.is_connected()) {
    app_registry_cache_.ForOneApp(
        app_id, [this, &permission](const apps::AppUpdate& update) {
          app_service_->SetPermission(update.AppType(), update.AppId(),
                                      std::move(permission));
        });
  }
}

void AppServiceProxyBase::UninstallSilently(
    const std::string& app_id,
    apps::mojom::UninstallSource uninstall_source) {
  if (app_service_.is_connected()) {
    apps::mojom::AppType app_type = app_registry_cache_.GetAppType(app_id);
    app_service_->Uninstall(app_type, app_id, uninstall_source,
                            /*clear_site_data=*/false, /*report_abuse=*/false);
    PerformPostUninstallTasks(app_type, app_id, uninstall_source);
  }
}

void AppServiceProxyBase::StopApp(const std::string& app_id) {
  if (!app_service_.is_connected()) {
    return;
  }
  apps::mojom::AppType app_type = app_registry_cache_.GetAppType(app_id);
  app_service_->StopApp(app_type, app_id);
}

void AppServiceProxyBase::GetMenuModel(
    const std::string& app_id,
    apps::mojom::MenuType menu_type,
    int64_t display_id,
    apps::mojom::Publisher::GetMenuModelCallback callback) {
  if (!app_service_.is_connected()) {
    return;
  }

  apps::mojom::AppType app_type = app_registry_cache_.GetAppType(app_id);
  app_service_->GetMenuModel(app_type, app_id, menu_type, display_id,
                             std::move(callback));
}

void AppServiceProxyBase::ExecuteContextMenuCommand(
    const std::string& app_id,
    int command_id,
    const std::string& shortcut_id,
    int64_t display_id) {
  if (!app_service_.is_connected()) {
    return;
  }

  apps::mojom::AppType app_type = app_registry_cache_.GetAppType(app_id);
  app_service_->ExecuteContextMenuCommand(app_type, app_id, command_id,
                                          shortcut_id, display_id);
}

void AppServiceProxyBase::OpenNativeSettings(const std::string& app_id) {
  if (app_service_.is_connected()) {
    app_registry_cache_.ForOneApp(
        app_id, [this](const apps::AppUpdate& update) {
          app_service_->OpenNativeSettings(update.AppType(), update.AppId());
        });
  }
}

apps::IconLoader* AppServiceProxyBase::OverrideInnerIconLoaderForTesting(
    apps::IconLoader* icon_loader) {
  apps::IconLoader* old =
      inner_icon_loader_.overriding_icon_loader_for_testing_;
  inner_icon_loader_.overriding_icon_loader_for_testing_ = icon_loader;
  return old;
}

std::vector<std::string> AppServiceProxyBase::GetAppIdsForUrl(
    const GURL& url,
    bool exclude_browsers,
    bool exclude_browser_tab_apps) {
  auto intent_launch_info =
      GetAppsForIntent(apps_util::CreateIntentFromUrl(url), exclude_browsers,
                       exclude_browser_tab_apps);
  std::vector<std::string> app_ids;
  for (auto& entry : intent_launch_info) {
    app_ids.push_back(std::move(entry.app_id));
  }
  return app_ids;
}

std::vector<IntentLaunchInfo> AppServiceProxyBase::GetAppsForIntent(
    const apps::mojom::IntentPtr& intent,
    bool exclude_browsers,
    bool exclude_browser_tab_apps) {
  std::vector<IntentLaunchInfo> intent_launch_info;
  if (apps_util::OnlyShareToDrive(intent) ||
      !apps_util::IsIntentValid(intent)) {
    return intent_launch_info;
  }

  if (app_service_.is_bound()) {
    app_registry_cache_.ForEachApp([&intent_launch_info, &intent,
                                    &exclude_browsers,
                                    &exclude_browser_tab_apps](
                                       const apps::AppUpdate& update) {
      if (update.Readiness() != apps::mojom::Readiness::kReady &&
          update.Readiness() != apps::mojom::Readiness::kDisabledByPolicy) {
        // We consider apps disabled by policy to be ready as they cause URL
        // loads to be blocked.
        return;
      }
      if (update.HandlesIntents() != apps::mojom::OptionalBool::kTrue) {
        return;
      }
      if (exclude_browser_tab_apps &&
          update.WindowMode() == mojom::WindowMode::kBrowser) {
        return;
      }
      // |activity_label| -> {index, is_generic}
      std::map<std::string, IndexAndGeneric> best_handler_map;
      bool is_file_handling_intent =
          intent->files.has_value() && intent->files->size() > 0;
      size_t index = 0;
      for (const auto& filter : update.IntentFilters()) {
        if (exclude_browsers && apps_util::IsBrowserFilter(filter)) {
          continue;
        }
        if (apps_util::IntentMatchesFilter(intent, filter)) {
          // Return the first non-generic match if it exists, otherwise the
          // first generic match.
          bool generic = false;
          if (is_file_handling_intent) {
            generic = apps_util::IsGenericFileHandler(intent, filter);
          }
          std::string activity_label = GetActivityLabel(filter, update);
          // Replace the best handler if it is generic and we have a non-generic
          // one.
          auto it = best_handler_map.find(activity_label);
          if (it == best_handler_map.end() ||
              (it->second.is_generic && !generic)) {
            best_handler_map[activity_label] = IndexAndGeneric{index, generic};
          }
        }
        index++;
      }
      const auto& filters = update.IntentFilters();
      for (const auto& handler_entry : best_handler_map) {
        const mojom::IntentFilterPtr& filter =
            filters[handler_entry.second.index];
        IntentLaunchInfo entry;
        entry.app_id = update.AppId();
        entry.activity_label = GetActivityLabel(filter, update);
        entry.activity_name = filter->activity_name.value_or("");
        entry.is_generic_file_handler =
            apps_util::IsGenericFileHandler(intent, filter);
        entry.is_file_extension_match =
            apps_util::FilterIsForFileExtensions(filter);
        intent_launch_info.push_back(entry);
      }
    });
  }
  return intent_launch_info;
}

std::vector<IntentLaunchInfo> AppServiceProxyBase::GetAppsForFiles(
    std::vector<apps::mojom::IntentFilePtr> files) {
  return GetAppsForIntent(
      apps_util::CreateViewIntentFromFiles(std::move(files)), false, false);
}

void AppServiceProxyBase::AddPreferredApp(const std::string& app_id,
                                          const GURL& url) {
  AddPreferredApp(app_id, apps_util::CreateIntentFromUrl(url));
}

void AppServiceProxyBase::AddPreferredApp(
    const std::string& app_id,
    const apps::mojom::IntentPtr& intent) {
  // TODO(https://crbug.com/853604): Remove this and convert to a DCHECK
  // after finding out the root cause.
  if (app_id.empty()) {
    base::debug::DumpWithoutCrashing();
    return;
  }
  auto intent_filter = FindBestMatchingFilter(intent);
  if (!intent_filter || !app_service_.is_connected()) {
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Link capturing behavior is changing with the launch of this feature so that
  // link capturing is enabled on a per app (rather than per-intent-filter)
  // basis. Non-Chrome OS platforms do not currently use persistent link
  // capturing preferences and so this is not a breaking change for them.
  bool supported_links_behavior_enabled =
      base::FeatureList::IsEnabled(features::kAppManagementIntentSettings);
#else
  bool supported_links_behavior_enabled = true;
#endif

  if (supported_links_behavior_enabled) {
    // Treat kUseBrowserForLink like an app with a single supported link, so
    // that any apps with overlapping supported links will have their preference
    // removed correctly.
    if (app_id == apps::kUseBrowserForLink) {
      std::vector<apps::mojom::IntentFilterPtr> filters;
      filters.push_back(std::move(intent_filter));
      app_service_->SetSupportedLinksPreference(apps::mojom::AppType::kUnknown,
                                                app_id, std::move(filters));
      return;
    }

    if (apps_util::IsSupportedLinkForApp(app_id, intent_filter)) {
      SetSupportedLinksPreference(app_id);
      return;
    }
  }

  preferred_apps_.AddPreferredApp(app_id, intent_filter);
  constexpr bool kFromPublisher = false;
  app_service_->AddPreferredApp(app_registry_cache_.GetAppType(app_id), app_id,
                                std::move(intent_filter), intent->Clone(),
                                kFromPublisher);
}

void AppServiceProxyBase::SetSupportedLinksPreference(
    const std::string& app_id) {
  DCHECK(!app_id.empty());
  if (!app_service_.is_connected()) {
    return;
  }

  std::vector<apps::mojom::IntentFilterPtr> filters;
  AppRegistryCache().ForOneApp(
      app_id, [&app_id, &filters](const AppUpdate& app) {
        for (auto& filter : app.IntentFilters()) {
          if (apps_util::IsSupportedLinkForApp(app_id, filter)) {
            filters.push_back(std::move(filter));
          }
        }
      });

  app_service_->SetSupportedLinksPreference(
      app_registry_cache_.GetAppType(app_id), app_id, std::move(filters));
}

void AppServiceProxyBase::RemoveSupportedLinksPreference(
    const std::string& app_id) {
  DCHECK(!app_id.empty());
  if (app_service_.is_connected()) {
    app_service_->RemoveSupportedLinksPreference(
        app_registry_cache_.GetAppType(app_id), app_id);
  }
}

void AppServiceProxyBase::SetWindowMode(const std::string& app_id,
                                        apps::mojom::WindowMode window_mode) {
  if (app_service_.is_connected()) {
    app_service_->SetWindowMode(app_registry_cache_.GetAppType(app_id), app_id,
                                window_mode);
  }
}

void AppServiceProxyBase::OnApps(std::vector<std::unique_ptr<apps::App>> deltas,
                                 apps::AppType app_type,
                                 bool should_notify_initialized) {
  // TODO(crbug.com/1253250): add RemovePreferredApp related code.
  app_registry_cache_.OnApps(std::move(deltas), app_type,
                             should_notify_initialized);
}

void AppServiceProxyBase::OnApps(std::vector<apps::mojom::AppPtr> deltas,
                                 apps::mojom::AppType app_type,
                                 bool should_notify_initialized) {
  if (app_service_.is_connected()) {
    for (const auto& delta : deltas) {
      if (delta->readiness != apps::mojom::Readiness::kUnknown &&
          !apps_util::IsInstalled(delta->readiness)) {
        app_service_->RemovePreferredApp(delta->app_type, delta->app_id);
      }
    }
  }

  app_registry_cache_.OnApps(std::move(deltas), app_type,
                             should_notify_initialized);
}

void AppServiceProxyBase::OnCapabilityAccesses(
    std::vector<apps::mojom::CapabilityAccessPtr> deltas) {
  app_capability_access_cache_.OnCapabilityAccesses(std::move(deltas));
}

void AppServiceProxyBase::Clone(
    mojo::PendingReceiver<apps::mojom::Subscriber> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void AppServiceProxyBase::OnPreferredAppsChanged(
    apps::mojom::PreferredAppChangesPtr changes) {
  preferred_apps_.ApplyBulkUpdate(std::move(changes));
}

void AppServiceProxyBase::InitializePreferredApps(
    PreferredAppsList::PreferredApps preferred_apps) {
  preferred_apps_.Init(preferred_apps);
}

apps::mojom::IntentFilterPtr AppServiceProxyBase::FindBestMatchingFilter(
    const apps::mojom::IntentPtr& intent) {
  apps::mojom::IntentFilterPtr best_matching_intent_filter;
  if (!app_service_.is_bound()) {
    return best_matching_intent_filter;
  }

  int best_match_level = apps_util::IntentFilterMatchLevel::kNone;
  app_registry_cache_.ForEachApp(
      [&intent, &best_match_level,
       &best_matching_intent_filter](const apps::AppUpdate& update) {
        for (const auto& filter : update.IntentFilters()) {
          if (!apps_util::IntentMatchesFilter(intent, filter)) {
            continue;
          }
          auto match_level = apps_util::GetFilterMatchLevel(filter);
          if (match_level <= best_match_level) {
            continue;
          }
          best_matching_intent_filter = filter->Clone();
          best_match_level = match_level;
        }
      });
  return best_matching_intent_filter;
}

void AppServiceProxyBase::PerformPostLaunchTasks(
    apps::mojom::LaunchSource launch_source) {}

void AppServiceProxyBase::RecordAppPlatformMetrics(
    Profile* profile,
    const apps::AppUpdate& update,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::LaunchContainer container) {}

void AppServiceProxyBase::PerformPostUninstallTasks(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    apps::mojom::UninstallSource uninstall_source) {}

void AppServiceProxyBase::OnLaunched(LaunchCallback callback,
                                     LaunchResult&& launch_result) {
  std::move(callback).Run(std::move(launch_result));
}

IntentLaunchInfo::IntentLaunchInfo() = default;
IntentLaunchInfo::~IntentLaunchInfo() = default;
IntentLaunchInfo::IntentLaunchInfo(const IntentLaunchInfo& other) = default;

}  // namespace apps
