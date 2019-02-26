// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/internal_app/internal_app_metadata.h"

#include <memory>

#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/ksv/keyboard_shortcut_viewer_util.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/discover_window_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_features_parser.h"
#include "components/arc/metrics/arc_metrics_constants.h"
#include "components/arc/property/arc_property_bridge.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/synced_session.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "url/gurl.h"

namespace app_list {

namespace {
constexpr char kChromeCameraAppId[] = "hfhhnacclhffhdffklopdkcgdhifgngh";

// Generated as ArcAppListPrefs::GetAppIdByPackageName(
//     "com.google.android.GoogleCameraArc").
constexpr char kAndroidCameraAppId[] = "goamfaniemdfcajgcmmflhchgkmbngka";
// Generated as ArcAppListPrefs::GetAppIdByPackageName(
//     "com.android.camera2").
constexpr char kAndroidLegacyCameraAppId[] = "obfofkigjfamlldmipdegnjlcpincibc";
// Generated as ArcAppListPrefs::GetAppIdByPackageName(
//     "com.android.googlecameramigration").
constexpr char kAndroidCameraMigrationAppId[] =
    "ngmkobaiicipbagcngcmilfkhejlnfci";

const std::vector<InternalApp>& GetInternalAppListImpl(bool get_all,
                                                       const Profile* profile) {
  DCHECK(get_all || profile);
  static const base::NoDestructor<std::vector<InternalApp>>
      internal_app_list_static(
          {{kInternalAppIdKeyboardShortcutViewer,
            IDS_INTERNAL_APP_KEYBOARD_SHORTCUT_VIEWER,
            IDR_SHORTCUT_VIEWER_LOGO_192,
            /*recommendable=*/false,
            /*searchable=*/true,
            /*show_in_launcher=*/false,
            InternalAppName::kKeyboardShortcutViewer,
            IDS_LAUNCHER_SEARCHABLE_KEYBOARD_SHORTCUT_VIEWER},

           {kInternalAppIdSettings, IDS_INTERNAL_APP_SETTINGS,
            IDR_SETTINGS_LOGO_192,
            /*recommendable=*/true,
            /*searchable=*/true,
            /*show_in_launcher=*/true, InternalAppName::kSettings,
            /*searchable_string_resource_id=*/0},

           {kInternalAppIdContinueReading, IDS_INTERNAL_APP_CONTINUOUS_READING,
            IDR_PRODUCT_LOGO_256,
            /*recommendable=*/true,
            /*searchable=*/false,
            /*show_in_launcher=*/false, InternalAppName::kContinueReading,
            /*searchable_string_resource_id=*/0}});

  static base::NoDestructor<std::vector<InternalApp>> internal_app_list;
  internal_app_list->clear();
  internal_app_list->insert(internal_app_list->begin(),
                            internal_app_list_static->begin(),
                            internal_app_list_static->end());

  const bool add_camera_app = get_all || !profile->IsGuestSession();
  if (add_camera_app) {
    internal_app_list->push_back(
        {kInternalAppIdCamera, IDS_INTERNAL_APP_CAMERA, IDR_CAMERA_LOGO_192,
         /*recommendable=*/true,
         /*searchable=*/true,
         /*show_in_launcher=*/true, InternalAppName::kCamera,
         /*searchable_string_resource_id=*/0});
  }

  const bool add_discover_app =
      get_all || !chromeos::ProfileHelper::IsEphemeralUserProfile(profile);
  if (base::FeatureList::IsEnabled(chromeos::features::kDiscoverApp) &&
      add_discover_app) {
    internal_app_list->push_back(
        {kInternalAppIdDiscover, IDS_INTERNAL_APP_DISCOVER,
         IDR_DISCOVER_APP_192,
         /*recommendable=*/false,
         /*searchable=*/true,
         /*show_in_launcher=*/true, InternalAppName::kDiscover,
         /*searchable_string_resource_id=*/IDS_INTERNAL_APP_DISCOVER});
  }
  return *internal_app_list;
}

}  // namespace

const std::vector<InternalApp>& GetInternalAppList(const Profile* profile) {
  return GetInternalAppListImpl(false, profile);
}

const InternalApp* FindInternalApp(const std::string& app_id) {
  for (const auto& app : GetInternalAppListImpl(true, nullptr)) {
    if (app_id == app.app_id)
      return &app;
  }
  return nullptr;
}

bool IsInternalApp(const std::string& app_id) {
  return !!FindInternalApp(app_id);
}

base::string16 GetInternalAppNameById(const std::string& app_id) {
  const auto* app = FindInternalApp(app_id);
  return app ? l10n_util::GetStringUTF16(app->name_string_resource_id)
             : base::string16();
}

int GetIconResourceIdByAppId(const std::string& app_id) {
  const auto* app = FindInternalApp(app_id);
  return app ? app->icon_resource_id : 0;
}

void OpenChromeCameraApp(Profile* profile,
                         int event_flags,
                         const extensions::Extension* extension) {
  AppListClientImpl* controller = AppListClientImpl::GetInstance();
  AppLaunchParams params = CreateAppLaunchParamsWithEventFlags(
      profile, extension, event_flags, extensions::SOURCE_APP_LAUNCHER,
      controller->GetAppListDisplayId());
  params.launch_id = ash::ShelfID(extension->id()).launch_id;
  OpenApplication(params);
  VLOG(1) << "Launched CCA.";
}

void ShowWebStore(Profile* profile,
                  int event_flags,
                  const std::string chrome_app_id) {
  AppListClientImpl* controller = AppListClientImpl::GetInstance();
  const GURL store_url = net::AppendQueryParameter(
      GURL(extension_urls::GetWebstoreItemDetailURLPrefix() + chrome_app_id),
      extension_urls::kWebstoreSourceField,
      extension_urls::kLaunchSourceAppListSearch);
  controller->OpenURL(profile, store_url, ui::PAGE_TRANSITION_LINK,
                      ui::DispositionFromEventFlags(event_flags));
  VLOG(1) << "Launched CWS.";
}

void OnGetMigrationProperty(Profile* profile,
                            int event_flags,
                            const base::Optional<std::string>& result) {
  const char* app_id = kAndroidCameraAppId;
  if (!result.has_value() || result.value() != "true") {
    VLOG(1) << "GCA migration is not finished. Launch migration app.";
    app_id = kAndroidCameraMigrationAppId;
  }

  AppListClientImpl* controller = AppListClientImpl::GetInstance();
  if (arc::LaunchApp(profile, app_id, event_flags,
                     arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER,
                     controller->GetAppListDisplayId())) {
    VLOG(1) << "Launched "
            << (app_id == kAndroidCameraAppId ? " GCA." : "GCA migration.");
    return;
  }
  if (arc::LaunchApp(profile, kAndroidLegacyCameraAppId, event_flags,
                     arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER,
                     controller->GetAppListDisplayId())) {
    VLOG(1) << "Launched legacy GCA.";
    return;
  }

  LOG(ERROR) << "Failed to launch any camera apps. Fallback to CCA.";
  const std::string chrome_app_id(kChromeCameraAppId);
  const extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  const extensions::Extension* extension =
      registry->GetInstalledExtension(chrome_app_id);
  if (extension)
    OpenChromeCameraApp(profile, event_flags, extension);
  else
    ShowWebStore(profile, event_flags, chrome_app_id);
}

bool IsArcPOrAbove(const base::Optional<arc::ArcFeatures>& read_result) {
  if (read_result == base::nullopt)
    return false;

  const std::string version_str =
      read_result.value().build_props.at("ro.build.version.sdk");
  int version = 0;
  VLOG(1) << "ARC version is " << version_str;
  return base::StringToInt(version_str, &version) && version >= 28;
}

void OnArcFeaturesRead(Profile* profile,
                       int event_flags,
                       base::Optional<arc::ArcFeatures> read_result) {
  bool arc_p_or_above = IsArcPOrAbove(read_result);
  const std::string chrome_app_id(kChromeCameraAppId);
  const extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  const extensions::Extension* extension =
      registry->GetInstalledExtension(chrome_app_id);

  bool arc_enabled = arc::IsArcPlayStoreEnabledForProfile(profile);
  bool is_android_camera_app_registered =
      arc_enabled &&
      ArcAppListPrefs::Get(profile)->IsRegistered(kAndroidCameraAppId);
  bool chrome_camera_migrated =
      profile->GetPrefs()->GetBoolean(prefs::kCameraMediaConsolidated);

  VLOG(1) << "Launching camera app. arc_enabled = " << arc_enabled
          << " is_android_camera_app_registered = "
          << is_android_camera_app_registered
          << " arc_p_or_above = " << arc_p_or_above
          << " chrome_camera_migrated = " << chrome_camera_migrated
          << " cca_exist = " << (extension != nullptr);
  if (arc_p_or_above && is_android_camera_app_registered &&
      (!extension || chrome_camera_migrated)) {
    // Open Google camera app or GCA migration app according to GCA
    // migration system property.
    arc::ArcPropertyBridge* property =
        arc::ArcPropertyBridge::GetForBrowserContext(profile);
    property->GetGcaMigrationProperty(
        base::BindOnce(&OnGetMigrationProperty, profile, event_flags));
  } else if (extension) {
    OpenChromeCameraApp(profile, event_flags, extension);
  } else {
    ShowWebStore(profile, event_flags, chrome_app_id);
  }
}

void OpenInternalApp(const std::string& app_id,
                     Profile* profile,
                     int event_flags) {
  if (app_id == kInternalAppIdKeyboardShortcutViewer) {
    keyboard_shortcut_viewer_util::ToggleKeyboardShortcutViewer();
  } else if (app_id == kInternalAppIdSettings) {
    chrome::ShowSettingsSubPageForProfile(profile, std::string());
  } else if (app_id == kInternalAppIdCamera) {
    arc::ArcFeaturesParser::GetArcFeatures(
        base::BindOnce(&OnArcFeaturesRead, profile, event_flags));
  } else if (app_id == kInternalAppIdDiscover) {
#if defined(OS_CHROMEOS)
    base::RecordAction(base::UserMetricsAction("ShowDiscover"));
    chromeos::DiscoverWindowManager::GetInstance()
        ->ShowChromeDiscoverPageForProfile(profile);
#endif
  }
}

gfx::ImageSkia GetIconForResourceId(int resource_id, int resource_size_in_dip) {
  if (resource_id == 0)
    return gfx::ImageSkia();

  gfx::ImageSkia* source =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id);
  return gfx::ImageSkiaOperations::CreateResizedImage(
      *source, skia::ImageOperations::RESIZE_BEST,
      gfx::Size(resource_size_in_dip, resource_size_in_dip));
}

bool HasRecommendableForeignTab(Profile* profile,
                                base::string16* title,
                                GURL* url) {
  sync_sessions::SessionSyncService* service =
      SessionSyncServiceFactory::GetForProfile(profile);
  std::vector<const sync_sessions::SyncedSession*> foreign_sessions;
  sync_sessions::OpenTabsUIDelegate* delegate =
      service->GetOpenTabsUIDelegate();
  if (delegate != nullptr)
    delegate->GetAllForeignSessions(&foreign_sessions);

  constexpr int kMaxForeignTabAgeInMinutes = 120;
  base::Time latest_timestamp;
  bool has_recommendation = false;
  for (const sync_sessions::SyncedSession* session : foreign_sessions) {
    if (latest_timestamp > session->modified_time)
      continue;

    auto device_type = session->device_type;
    if (device_type != sync_pb::SyncEnums::TYPE_PHONE &&
        device_type != sync_pb::SyncEnums::TYPE_TABLET) {
      continue;
    }

    for (const auto& key_value : session->windows) {
      for (const std::unique_ptr<sessions::SessionTab>& tab :
           key_value.second->wrapped_window.tabs) {
        if (tab->navigations.empty())
          continue;

        const sessions::SerializedNavigationEntry& navigation =
            tab->navigations.back();
        const GURL& virtual_url = navigation.virtual_url();

        // Only show pages with http or https.
        if (!virtual_url.SchemeIsHTTPOrHTTPS())
          continue;

        // Only show pages recently opened.
        const base::TimeDelta tab_age = base::Time::Now() - tab->timestamp;
        if (tab_age > base::TimeDelta::FromMinutes(kMaxForeignTabAgeInMinutes))
          continue;

        if (latest_timestamp < tab->timestamp) {
          has_recommendation = true;
          latest_timestamp = tab->timestamp;
          if (title)
            *title = navigation.title();

          if (url)
            *url = virtual_url;
        }
      }
    }
  }
  return has_recommendation;
}

InternalAppName GetInternalAppNameByAppId(
    const std::string& app_id) {
  const auto* app = FindInternalApp(app_id);
  DCHECK(app);
  return app->internal_app_name;
}

size_t GetNumberOfInternalAppsShowInLauncherForTest(std::string* apps_name,
                                                    const Profile* profile) {
  size_t num_of_internal_apps_show_in_launcher = 0u;
  std::vector<std::string> internal_apps_name;
  for (const auto& app : GetInternalAppList(profile)) {
    if (app.show_in_launcher) {
      ++num_of_internal_apps_show_in_launcher;
      if (apps_name) {
        internal_apps_name.emplace_back(
            l10n_util::GetStringUTF8(app.name_string_resource_id));
      }
    }
  }
  if (apps_name)
    *apps_name = base::JoinString(internal_apps_name, ",");
  return num_of_internal_apps_show_in_launcher;
}

}  // namespace app_list
