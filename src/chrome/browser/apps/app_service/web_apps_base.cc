// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/web_apps_base.h"

#include <utility>
#include <vector>

#include "base/callback.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_manager.h"
#include "chrome/browser/ui/web_applications/web_app_launch_manager.h"
#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/services/app_service/public/cpp/intent_filter_util.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_contents.h"

namespace {

// Only supporting important permissions for now.
const ContentSettingsType kSupportedPermissionTypes[] = {
    ContentSettingsType::MEDIASTREAM_MIC,
    ContentSettingsType::MEDIASTREAM_CAMERA,
    ContentSettingsType::GEOLOCATION,
    ContentSettingsType::NOTIFICATIONS,
};

apps::mojom::InstallSource GetHighestPriorityInstallSource(
    const web_app::WebApp* web_app) {
  switch (web_app->GetHighestPrioritySource()) {
    case web_app::Source::kSystem:
      return apps::mojom::InstallSource::kSystem;
    case web_app::Source::kPolicy:
      return apps::mojom::InstallSource::kPolicy;
    case web_app::Source::kWebAppStore:
      return apps::mojom::InstallSource::kUser;
    case web_app::Source::kSync:
      return apps::mojom::InstallSource::kUser;
    case web_app::Source::kDefault:
      return apps::mojom::InstallSource::kDefault;
  }
}

}  // namespace

namespace apps {

WebAppsBase::WebAppsBase(
    const mojo::Remote<apps::mojom::AppService>& app_service,
    Profile* profile)
    : profile_(profile), app_service_(nullptr) {
  Initialize(app_service);
}

WebAppsBase::~WebAppsBase() = default;

void WebAppsBase::Shutdown() {
  if (provider_) {
    registrar_observer_.Remove(&provider_->registrar());
    content_settings_observer_.RemoveAll();
  }
}

const web_app::WebApp* WebAppsBase::GetWebApp(
    const web_app::AppId& app_id) const {
  return GetRegistrar().GetAppById(app_id);
}

void WebAppsBase::OnWebAppUninstalled(const web_app::AppId& app_id) {
  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (!web_app || !Accepts(app_id)) {
    return;
  }

  // Construct an App with only the information required to identify an
  // uninstallation.
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kWeb;
  app->app_id = web_app->app_id();
  // TODO(loyso): Plumb uninstall source (reason) here.
  app->readiness = apps::mojom::Readiness::kUninstalledByUser;

  SetShowInFields(app, web_app);
  Publish(std::move(app), subscribers_);

  if (app_service_) {
    app_service_->RemovePreferredApp(apps::mojom::AppType::kWeb,
                                     web_app->app_id());
  }
}

apps::mojom::AppPtr WebAppsBase::ConvertImpl(const web_app::WebApp* web_app,
                                             apps::mojom::Readiness readiness) {
  apps::mojom::AppPtr app = PublisherBase::MakeApp(
      apps::mojom::AppType::kWeb, web_app->app_id(), readiness, web_app->name(),
      GetHighestPriorityInstallSource(web_app));

  app->description = web_app->description();
  app->additional_search_terms = web_app->additional_search_terms();

  // app->version is left empty here.
  // TODO(loyso): Populate app->last_launch_time and app->install_time.

  PopulatePermissions(web_app, &app->permissions);

  SetShowInFields(app, web_app);

  // Get the intent filters for PWAs.
  PopulateIntentFilters(GetRegistrar().GetAppScope(web_app->app_id()),
                        &app->intent_filters);

  return app;
}

IconEffects WebAppsBase::GetIconEffects(const web_app::WebApp* web_app) {
  IconEffects icon_effects = IconEffects::kNone;
  if (!web_app->is_locally_installed()) {
    icon_effects =
        static_cast<IconEffects>(icon_effects | IconEffects::kBlocked);
  }
  icon_effects =
      static_cast<IconEffects>(icon_effects | IconEffects::kRoundCorners);
  return icon_effects;
}

content::WebContents* WebAppsBase::LaunchAppWithIntentImpl(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::IntentPtr intent,
    apps::mojom::LaunchSource launch_source,
    int64_t display_id) {
  if (!profile_) {
    return nullptr;
  }

  auto params = apps::CreateAppLaunchParamsForIntent(
      app_id, event_flags, GetAppLaunchSource(launch_source), display_id,
      web_app::ConvertDisplayModeToAppLaunchContainer(
          GetRegistrar().GetAppEffectiveDisplayMode(app_id)),
      intent);
  return web_app_launch_manager_->OpenApplication(params);
}

void WebAppsBase::Initialize(
    const mojo::Remote<apps::mojom::AppService>& app_service) {
  DCHECK(profile_);
  if (!web_app::AreWebAppsEnabled(profile_)) {
    return;
  }

  provider_ = web_app::WebAppProvider::Get(profile_);
  DCHECK(provider_);

  registrar_observer_.Add(&provider_->registrar());
  content_settings_observer_.Add(
      HostContentSettingsMapFactory::GetForProfile(profile_));

  web_app_launch_manager_ =
      std::make_unique<web_app::WebAppLaunchManager>(profile_);

  PublisherBase::Initialize(app_service, apps::mojom::AppType::kWeb);
  app_service_ = app_service.get();
}

const web_app::WebAppRegistrar& WebAppsBase::GetRegistrar() const {
  DCHECK(provider_);

  // TODO(loyso): Remove this downcast after bookmark apps erasure.
  web_app::WebAppSyncBridge* sync_bridge =
      provider_->registry_controller().AsWebAppSyncBridge();
  DCHECK(sync_bridge);
  return sync_bridge->registrar();
}

void WebAppsBase::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  DCHECK(provider_);

  provider_->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&WebAppsBase::StartPublishingWebApps,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::move(subscriber_remote)));
}

void WebAppsBase::LoadIcon(const std::string& app_id,
                           apps::mojom::IconKeyPtr icon_key,
                           apps::mojom::IconCompression icon_compression,
                           int32_t size_hint_in_dip,
                           bool allow_placeholder_icon,
                           LoadIconCallback callback) {
  DCHECK(provider_);

  if (icon_key) {
    LoadIconFromWebApp(profile_, icon_compression, size_hint_in_dip, app_id,
                       static_cast<IconEffects>(icon_key->icon_effects),
                       std::move(callback));
    return;
  }
  // On failure, we still run the callback, with the zero IconValue.
  std::move(callback).Run(apps::mojom::IconValue::New());
}

void WebAppsBase::Launch(const std::string& app_id,
                         int32_t event_flags,
                         apps::mojom::LaunchSource launch_source,
                         int64_t display_id) {
  if (!profile_) {
    return;
  }

  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  // TODO(loyso): Record UMA_HISTOGRAM_ENUMERATION here based on launch_source.

  web_app::DisplayMode display_mode =
      GetRegistrar().GetAppEffectiveDisplayMode(app_id);

  AppLaunchParams params = apps::CreateAppIdLaunchParamsWithEventFlags(
      web_app->app_id(), event_flags, GetAppLaunchSource(launch_source),
      display_id,
      /*fallback_container=*/
      web_app::ConvertDisplayModeToAppLaunchContainer(display_mode));

  // The app will be created for the currently active profile.
  web_app_launch_manager_->OpenApplication(params);
}

void WebAppsBase::LaunchAppWithFiles(const std::string& app_id,
                                     apps::mojom::LaunchContainer container,
                                     int32_t event_flags,
                                     apps::mojom::LaunchSource launch_source,
                                     apps::mojom::FilePathsPtr file_paths) {
  apps::AppLaunchParams params(
      app_id, container, ui::DispositionFromEventFlags(event_flags),
      GetAppLaunchSource(launch_source), display::kDefaultDisplayId);
  for (const auto& file_path : file_paths->file_paths) {
    params.launch_files.push_back(file_path);
  }

  // The app will be created for the currently active profile.
  web_app_launch_manager_->OpenApplication(params);
}

void WebAppsBase::LaunchAppWithIntent(const std::string& app_id,
                                      int32_t event_flags,
                                      apps::mojom::IntentPtr intent,
                                      apps::mojom::LaunchSource launch_source,
                                      int64_t display_id) {
  LaunchAppWithIntentImpl(app_id, event_flags, std::move(intent), launch_source,
                          display_id);
}

void WebAppsBase::SetPermission(const std::string& app_id,
                                apps::mojom::PermissionPtr permission) {
  if (!profile_) {
    return;
  }

  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  DCHECK(host_content_settings_map);

  const GURL url = web_app->launch_url();

  ContentSettingsType permission_type =
      static_cast<ContentSettingsType>(permission->permission_id);
  if (!base::Contains(kSupportedPermissionTypes, permission_type)) {
    return;
  }

  DCHECK_EQ(permission->value_type,
            apps::mojom::PermissionValueType::kTriState);
  ContentSetting permission_value = CONTENT_SETTING_DEFAULT;
  switch (static_cast<apps::mojom::TriState>(permission->value)) {
    case apps::mojom::TriState::kAllow:
      permission_value = CONTENT_SETTING_ALLOW;
      break;
    case apps::mojom::TriState::kAsk:
      permission_value = CONTENT_SETTING_ASK;
      break;
    case apps::mojom::TriState::kBlock:
      permission_value = CONTENT_SETTING_BLOCK;
      break;
    default:  // Return if value is invalid.
      return;
  }

  host_content_settings_map->SetContentSettingDefaultScope(
      url, url, permission_type, /*resource_identifier=*/std::string(),
      permission_value);
}

void WebAppsBase::OpenNativeSettings(const std::string& app_id) {
  if (!profile_) {
    return;
  }

  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  chrome::ShowSiteSettings(profile_, web_app->launch_url());
}

void WebAppsBase::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const std::string& resource_identifier) {
  // If content_type is not one of the supported permissions, do nothing.
  if (!base::Contains(kSupportedPermissionTypes, content_type)) {
    return;
  }

  if (!profile_) {
    return;
  }

  for (const web_app::WebApp& web_app : GetRegistrar().AllApps()) {
    if (web_app.is_in_sync_install()) {
      continue;
    }

    if (primary_pattern.Matches(web_app.launch_url()) &&
        Accepts(web_app.app_id())) {
      apps::mojom::AppPtr app = apps::mojom::App::New();
      app->app_type = apps::mojom::AppType::kWeb;
      app->app_id = web_app.app_id();
      PopulatePermissions(&web_app, &app->permissions);

      Publish(std::move(app), subscribers_);
    }
  }
}

void WebAppsBase::OnWebAppInstalled(const web_app::AppId& app_id) {
  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (web_app && Accepts(app_id)) {
    Publish(Convert(web_app, apps::mojom::Readiness::kReady), subscribers_);
  }
}

void WebAppsBase::OnAppRegistrarDestroyed() {
  registrar_observer_.RemoveAll();
}

void WebAppsBase::OnWebAppLocallyInstalledStateChanged(
    const web_app::AppId& app_id,
    bool is_locally_installed) {
  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (!web_app)
    return;
  auto app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kWeb;
  app->app_id = app_id;
  app->icon_key = icon_key_factory().MakeIconKey(GetIconEffects(web_app));
  Publish(std::move(app), subscribers_);
}

void WebAppsBase::SetShowInFields(apps::mojom::AppPtr& app,
                                  const web_app::WebApp* web_app) {
  if (web_app->chromeos_data().has_value()) {
    auto& chromeos_data = web_app->chromeos_data().value();
    app->show_in_launcher = chromeos_data.show_in_launcher
                                ? apps::mojom::OptionalBool::kTrue
                                : apps::mojom::OptionalBool::kFalse;
    app->show_in_search = chromeos_data.show_in_search
                              ? apps::mojom::OptionalBool::kTrue
                              : apps::mojom::OptionalBool::kFalse;
    app->show_in_management = chromeos_data.show_in_management
                                  ? apps::mojom::OptionalBool::kTrue
                                  : apps::mojom::OptionalBool::kFalse;
    return;
  }

  // Show the app everywhere by default.
  auto show = apps::mojom::OptionalBool::kTrue;
  app->show_in_launcher = show;
  app->show_in_search = show;
  app->show_in_management = show;
}

void WebAppsBase::PopulatePermissions(
    const web_app::WebApp* web_app,
    std::vector<mojom::PermissionPtr>* target) {
  const GURL url = web_app->launch_url();

  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  DCHECK(host_content_settings_map);

  for (ContentSettingsType type : kSupportedPermissionTypes) {
    ContentSetting setting = host_content_settings_map->GetContentSetting(
        url, url, type, /*resource_identifier=*/std::string());

    // Map ContentSettingsType to an apps::mojom::TriState value
    apps::mojom::TriState setting_val;
    switch (setting) {
      case CONTENT_SETTING_ALLOW:
        setting_val = apps::mojom::TriState::kAllow;
        break;
      case CONTENT_SETTING_ASK:
        setting_val = apps::mojom::TriState::kAsk;
        break;
      case CONTENT_SETTING_BLOCK:
        setting_val = apps::mojom::TriState::kBlock;
        break;
      default:
        setting_val = apps::mojom::TriState::kAsk;
    }

    content_settings::SettingInfo setting_info;
    host_content_settings_map->GetWebsiteSetting(url, url, type, std::string(),
                                                 &setting_info);

    auto permission = apps::mojom::Permission::New();
    permission->permission_id = static_cast<uint32_t>(type);
    permission->value_type = apps::mojom::PermissionValueType::kTriState;
    permission->value = static_cast<uint32_t>(setting_val);
    permission->is_managed =
        setting_info.source == content_settings::SETTING_SOURCE_POLICY;

    target->push_back(std::move(permission));
  }
}

void WebAppsBase::PopulateIntentFilters(
    const base::Optional<GURL>& app_scope,
    std::vector<mojom::IntentFilterPtr>* target) {
  if (app_scope != base::nullopt) {
    target->push_back(
        apps_util::CreateIntentFilterForUrlScope(app_scope.value()));
  }
}

void WebAppsBase::ConvertWebApps(apps::mojom::Readiness readiness,
                                 std::vector<apps::mojom::AppPtr>* apps_out) {
  for (const web_app::WebApp& web_app : GetRegistrar().AllApps()) {
    if (!web_app.is_in_sync_install()) {
      apps_out->push_back(Convert(&web_app, readiness));
    }
  }
}

void WebAppsBase::StartPublishingWebApps(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote) {
  std::vector<apps::mojom::AppPtr> apps;
  ConvertWebApps(apps::mojom::Readiness::kReady, &apps);

  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  subscriber->OnApps(std::move(apps));
  subscribers_.Add(std::move(subscriber));
}

}  // namespace apps
