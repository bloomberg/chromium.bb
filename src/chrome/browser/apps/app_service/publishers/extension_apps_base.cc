// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/extension_apps.h"

#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_observation.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/extension_uninstaller.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/publishers/extension_apps_enable_flow.h"
#include "chrome/browser/apps/app_service/publishers/extension_apps_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/extension_app_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/extensions/extension_enable_flow.h"
#include "chrome/browser/ui/extensions/extension_enable_flow_delegate.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "content/public/browser/clear_site_data_utils.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/ui_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/switches.h"
#include "net/base/url_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/url_constants.h"

// TODO(crbug.com/826982): life cycle events. Extensions can be installed and
// uninstalled. ExtensionAppsBase should implement extensions::InstallObserver
// and be able to show download progress in the UI, a la
// ExtensionAppModelBuilder. This might involve using an
// extensions::InstallTracker. It might also need the equivalent of a
// ShelfExtensionAppUpdater.

// TODO(crbug.com/826982): consider that, per khmel@, "in some places Chrome
// apps is not used and raw extension app without any effect is displayed...
// Search where ChromeAppIcon or ChromeAppIconLoader is used compared with
// direct loading the ExtensionIcon".

namespace {

std::string GetSourceFromAppListSource(ash::ShelfLaunchSource source) {
  switch (source) {
    case ash::LAUNCH_FROM_APP_LIST:
      return std::string(extension_urls::kLaunchSourceAppList);
    case ash::LAUNCH_FROM_APP_LIST_SEARCH:
      return std::string(extension_urls::kLaunchSourceAppListSearch);
    default:
      return std::string();
  }
}

ash::ShelfLaunchSource ConvertLaunchSource(
    apps::mojom::LaunchSource launch_source) {
  switch (launch_source) {
    case apps::mojom::LaunchSource::kUnknown:
    case apps::mojom::LaunchSource::kFromParentalControls:
      return ash::LAUNCH_FROM_UNKNOWN;
    case apps::mojom::LaunchSource::kFromAppListGrid:
    case apps::mojom::LaunchSource::kFromAppListGridContextMenu:
      return ash::LAUNCH_FROM_APP_LIST;
    case apps::mojom::LaunchSource::kFromAppListQuery:
    case apps::mojom::LaunchSource::kFromAppListQueryContextMenu:
    case apps::mojom::LaunchSource::kFromAppListRecommendation:
      return ash::LAUNCH_FROM_APP_LIST_SEARCH;
    case apps::mojom::LaunchSource::kFromShelf:
      return ash::LAUNCH_FROM_SHELF;
    case apps::mojom::LaunchSource::kFromFileManager:
    case apps::mojom::LaunchSource::kFromLink:
    case apps::mojom::LaunchSource::kFromOmnibox:
    case apps::mojom::LaunchSource::kFromChromeInternal:
    case apps::mojom::LaunchSource::kFromKeyboard:
    case apps::mojom::LaunchSource::kFromOtherApp:
    case apps::mojom::LaunchSource::kFromMenu:
    case apps::mojom::LaunchSource::kFromInstalledNotification:
    case apps::mojom::LaunchSource::kFromTest:
    case apps::mojom::LaunchSource::kFromArc:
    case apps::mojom::LaunchSource::kFromSharesheet:
    case apps::mojom::LaunchSource::kFromReleaseNotesNotification:
    case apps::mojom::LaunchSource::kFromFullRestore:
    case apps::mojom::LaunchSource::kFromSmartTextContextMenu:
    case apps::mojom::LaunchSource::kFromDiscoverTabNotification:
    case apps::mojom::LaunchSource::kFromManagementApi:
    case apps::mojom::LaunchSource::kFromKiosk:
    case apps::mojom::LaunchSource::kFromCommandLine:
    case apps::mojom::LaunchSource::kFromBackgroundMode:
    case apps::mojom::LaunchSource::kFromNewTabPage:
    case apps::mojom::LaunchSource::kFromIntentUrl:
    case apps::mojom::LaunchSource::kFromOsLogin:
    case apps::mojom::LaunchSource::kFromProtocolHandler:
    case apps::mojom::LaunchSource::kFromUrlHandler:
      return ash::LAUNCH_FROM_UNKNOWN;
  }
}

apps::mojom::InstallReason GetInstallReason(
    const Profile* profile,
    const extensions::Extension* extension) {
  if (extensions::Manifest::IsComponentLocation(extension->location())) {
    return apps::mojom::InstallReason::kSystem;
  }

  if (extensions::Manifest::IsPolicyLocation(extension->location())) {
    return apps::mojom::InstallReason::kPolicy;
  }

  if (extension->was_installed_by_oem()) {
    return apps::mojom::InstallReason::kOem;
  }

  if (extension->was_installed_by_default()) {
    return apps::mojom::InstallReason::kDefault;
  }

  return apps::mojom::InstallReason::kUser;
}

}  // namespace

namespace apps {

ExtensionAppsBase::ExtensionAppsBase(
    const mojo::Remote<apps::mojom::AppService>& app_service,
    Profile* profile)
    : profile_(profile), app_service_(nullptr) {
  Initialize(app_service);
}

ExtensionAppsBase::~ExtensionAppsBase() = default;

void ExtensionAppsBase::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  // If the extension doesn't belong to this publisher, do nothing.
  if (!Accepts(extension)) {
    return;
  }

  enable_flow_map_.erase(extension->id());

  // Construct an App with only the information required to identify an
  // uninstallation.
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kExtension;
  app->app_id = extension->id();
  app->readiness = reason == extensions::UNINSTALL_REASON_MIGRATED
                       ? apps::mojom::Readiness::kUninstalledByMigration
                       : apps::mojom::Readiness::kUninstalledByUser;

  SetShowInFields(app, extension);
  Publish(std::move(app), subscribers_);
}

void ExtensionAppsBase::SetShowInFields(
    apps::mojom::AppPtr& app,
    const extensions::Extension* extension) {
  if (ShouldShow(extension, profile_)) {
    auto show = ShouldShownInLauncher(extension)
                    ? apps::mojom::OptionalBool::kTrue
                    : apps::mojom::OptionalBool::kFalse;
    app->show_in_launcher = show;
    app->show_in_shelf = show;
    app->show_in_search = show;
    app->show_in_management = show;
  } else {
    app->show_in_launcher = apps::mojom::OptionalBool::kFalse;
    app->show_in_shelf = apps::mojom::OptionalBool::kFalse;
    app->show_in_search = apps::mojom::OptionalBool::kFalse;
    app->show_in_management = apps::mojom::OptionalBool::kFalse;
  }
}

apps::mojom::AppPtr ExtensionAppsBase::ConvertImpl(
    const extensions::Extension* extension,
    apps::mojom::Readiness readiness) {
  auto install_reason = GetInstallReason(profile_, extension);
  apps::mojom::AppPtr app =
      PublisherBase::MakeApp(apps::mojom::AppType::kExtension, extension->id(),
                             readiness, extension->name(), install_reason);

  app->short_name = extension->short_name();
  app->description = extension->description();
  app->version = extension->GetVersionForDisplay();

  if (profile_) {
    auto* prefs = extensions::ExtensionPrefs::Get(profile_);
    if (prefs) {
      app->last_launch_time = prefs->GetLastLaunchTime(extension->id());
      app->install_time = prefs->GetInstallTime(extension->id());
    }
  }
  app->install_source = install_reason == apps::mojom::InstallReason::kSystem
                            ? apps::mojom::InstallSource::kSystem
                            : apps::mojom::InstallSource::kChromeWebStore;

  app->is_platform_app = extension->is_platform_app()
                             ? apps::mojom::OptionalBool::kTrue
                             : apps::mojom::OptionalBool::kFalse;

  SetShowInFields(app, extension);

  const extensions::ManagementPolicy* policy =
      extensions::ExtensionSystem::Get(profile())->management_policy();
  DCHECK(policy);
  app->allow_uninstall = (policy->UserMayModifySettings(extension, nullptr) &&
                          !policy->MustRemainInstalled(extension, nullptr))
                             ? apps::mojom::OptionalBool::kTrue
                             : apps::mojom::OptionalBool::kFalse;

  return app;
}

IconEffects ExtensionAppsBase::GetIconEffects(
    const extensions::Extension* extension) {
  IconEffects icon_effects = IconEffects::kNone;
  if (!extensions::util::IsAppLaunchable(extension->id(), profile_)) {
    icon_effects =
        static_cast<IconEffects>(icon_effects | IconEffects::kBlocked);
  }

  return icon_effects;
}

content::WebContents* ExtensionAppsBase::LaunchAppWithIntentImpl(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::IntentPtr intent,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::WindowInfoPtr window_info,
    LaunchAppWithIntentCallback callback) {
  const auto* extension = MaybeGetExtension(app_id);
  if (!extension || !extensions::util::IsAppLaunchable(app_id, profile_)) {
    std::move(callback).Run(/*success=*/false);
    return nullptr;
  }

  if (!extensions::util::IsAppLaunchableWithoutEnabling(app_id, profile_)) {
    RunExtensionEnableFlow(
        app_id,
        base::BindOnce(&ExtensionAppsBase::ExtensionWasEnabled,
                       weak_factory_.GetWeakPtr(), app_id, event_flags,
                       std::move(intent), launch_source, std::move(window_info),
                       CallbackWrapper(std::move(callback))));
    return nullptr;
  }

  auto params = apps::CreateAppLaunchParamsForIntent(
      app_id, event_flags, launch_source,
      window_info ? window_info->display_id : display::kInvalidDisplayId,
      extensions::GetLaunchContainer(extensions::ExtensionPrefs::Get(profile_),
                                     extension),
      std::move(intent), profile_);
  std::move(callback).Run(/*success=*/true);
  return LaunchImpl(std::move(params));
}

content::WebContents* ExtensionAppsBase::LaunchImpl(AppLaunchParams&& params) {
  return ::OpenApplication(profile_, std::move(params));
}

const extensions::Extension* ExtensionAppsBase::MaybeGetExtension(
    const std::string& app_id) {
  DCHECK(profile_);
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile_);
  DCHECK(registry);
  const extensions::Extension* extension =
      registry->GetInstalledExtension(app_id);
  if (!extension || !Accepts(extension)) {
    return nullptr;
  }

  return extension;
}

ExtensionAppsBase::CallbackWrapper::CallbackWrapper(
    base::OnceCallback<void(bool)> callback)
    : callback(std::move(callback)) {}

ExtensionAppsBase::CallbackWrapper::CallbackWrapper(CallbackWrapper&&) =
    default;

ExtensionAppsBase::CallbackWrapper::~CallbackWrapper() {
  if (callback)
    std::move(callback).Run(false);
}

void ExtensionAppsBase::Initialize(
    const mojo::Remote<apps::mojom::AppService>& app_service) {
  DCHECK(profile_);
  PublisherBase::Initialize(app_service, apps::mojom::AppType::kExtension);

  prefs_observation_.Observe(extensions::ExtensionPrefs::Get(profile_));
  registry_observation_.Observe(extensions::ExtensionRegistry::Get(profile_));
  app_service_ = app_service.get();
}

void ExtensionAppsBase::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  // TODO(crbug.com/1030126): Start publishing Extension Apps asynchronously on
  // ExtensionSystem::Get(profile())->ready().
  std::vector<apps::mojom::AppPtr> apps;
  if (profile_) {
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile_);
    ConvertVector(registry->enabled_extensions(),
                  apps::mojom::Readiness::kReady, &apps);
    ConvertVector(registry->disabled_extensions(),
                  apps::mojom::Readiness::kDisabledByUser, &apps);
    ConvertVector(registry->terminated_extensions(),
                  apps::mojom::Readiness::kTerminated, &apps);
    // blocklisted_extensions and blocked_extensions, corresponding to
    // kDisabledByBlocklist and kDisabledByPolicy, are deliberately ignored.
    //
    // If making changes to which sets are consulted, also change ShouldShow,
    // OnHideWebStoreIconPrefChanged.
  }
  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  subscriber->OnApps(std::move(apps), apps::mojom::AppType::kExtension,
                     true /* should_notify_initialized */);
  subscribers_.Add(std::move(subscriber));
}

void ExtensionAppsBase::LoadIcon(const std::string& app_id,
                                 apps::mojom::IconKeyPtr icon_key,
                                 apps::mojom::IconType icon_type,
                                 int32_t size_hint_in_dip,
                                 bool allow_placeholder_icon,
                                 LoadIconCallback callback) {
  if (icon_key) {
    LoadIconFromExtension(
        ConvertMojomIconTypeToIconType(icon_type), size_hint_in_dip, profile_,
        app_id, static_cast<IconEffects>(icon_key->icon_effects),
        IconValueToMojomIconValueCallback(std::move(callback)));
    return;
  }
  // On failure, we still run the callback, with the zero IconValue.
  std::move(callback).Run(apps::mojom::IconValue::New());
}

void ExtensionAppsBase::Launch(const std::string& app_id,
                               int32_t event_flags,
                               apps::mojom::LaunchSource launch_source,
                               apps::mojom::WindowInfoPtr window_info) {
  const auto* extension = MaybeGetExtension(app_id);
  if (!extension || !extensions::util::IsAppLaunchable(app_id, profile_)) {
    return;
  }

  if (!extensions::util::IsAppLaunchableWithoutEnabling(app_id, profile_)) {
    RunExtensionEnableFlow(
        app_id, base::BindOnce(&ExtensionAppsBase::Launch,
                               weak_factory_.GetWeakPtr(), app_id, event_flags,
                               launch_source, std::move(window_info)));
    return;
  }

  switch (launch_source) {
    case apps::mojom::LaunchSource::kUnknown:
    case apps::mojom::LaunchSource::kFromParentalControls:
      break;
    case apps::mojom::LaunchSource::kFromAppListGrid:
    case apps::mojom::LaunchSource::kFromAppListGridContextMenu:
      extensions::RecordAppListMainLaunch(extension);
      break;
    case apps::mojom::LaunchSource::kFromAppListQuery:
    case apps::mojom::LaunchSource::kFromAppListQueryContextMenu:
      extensions::RecordAppListSearchLaunch(extension);
      break;
    case apps::mojom::LaunchSource::kFromAppListRecommendation:
    case apps::mojom::LaunchSource::kFromShelf:
    case apps::mojom::LaunchSource::kFromFileManager:
    case apps::mojom::LaunchSource::kFromLink:
    case apps::mojom::LaunchSource::kFromOmnibox:
    case apps::mojom::LaunchSource::kFromChromeInternal:
    case apps::mojom::LaunchSource::kFromKeyboard:
    case apps::mojom::LaunchSource::kFromOtherApp:
    case apps::mojom::LaunchSource::kFromMenu:
    case apps::mojom::LaunchSource::kFromInstalledNotification:
    case apps::mojom::LaunchSource::kFromTest:
    case apps::mojom::LaunchSource::kFromArc:
    case apps::mojom::LaunchSource::kFromSharesheet:
    case apps::mojom::LaunchSource::kFromReleaseNotesNotification:
    case apps::mojom::LaunchSource::kFromFullRestore:
    case apps::mojom::LaunchSource::kFromSmartTextContextMenu:
    case apps::mojom::LaunchSource::kFromDiscoverTabNotification:
    case apps::mojom::LaunchSource::kFromManagementApi:
    case apps::mojom::LaunchSource::kFromKiosk:
    case apps::mojom::LaunchSource::kFromCommandLine:
    case apps::mojom::LaunchSource::kFromBackgroundMode:
    case apps::mojom::LaunchSource::kFromNewTabPage:
    case apps::mojom::LaunchSource::kFromIntentUrl:
    case apps::mojom::LaunchSource::kFromOsLogin:
    case apps::mojom::LaunchSource::kFromProtocolHandler:
    case apps::mojom::LaunchSource::kFromUrlHandler:
      break;
  }

  // The app will be created for the currently active profile.
  AppLaunchParams params = CreateAppLaunchParamsWithEventFlags(
      profile_, extension, event_flags, launch_source,
      window_info ? window_info->display_id : display::kInvalidDisplayId);
  ash::ShelfLaunchSource source = ConvertLaunchSource(launch_source);
  if ((source == ash::LAUNCH_FROM_APP_LIST ||
       source == ash::LAUNCH_FROM_APP_LIST_SEARCH) &&
      app_id == extensions::kWebStoreAppId) {
    // Get the corresponding source string.
    std::string source_value = GetSourceFromAppListSource(source);

    // Set an override URL to include the source.
    GURL extension_url = extensions::AppLaunchInfo::GetFullLaunchURL(extension);
    params.override_url = net::AppendQueryParameter(
        extension_url, extension_urls::kWebstoreSourceField, source_value);
  }

  LaunchImpl(std::move(params));
}

void ExtensionAppsBase::LaunchAppWithFiles(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::FilePathsPtr file_paths) {
  const auto* extension = MaybeGetExtension(app_id);
  AppLaunchParams params(
      app_id,
      extensions::GetLaunchContainer(extensions::ExtensionPrefs::Get(profile_),
                                     extension),
      ui::DispositionFromEventFlags(event_flags), launch_source,
      display::kDefaultDisplayId);
  for (const auto& file_path : file_paths->file_paths) {
    params.launch_files.push_back(file_path);
  }
  LaunchImpl(std::move(params));
}

void ExtensionAppsBase::LaunchAppWithIntent(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::IntentPtr intent,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::WindowInfoPtr window_info,
    LaunchAppWithIntentCallback callback) {
  LaunchAppWithIntentImpl(app_id, event_flags, std::move(intent), launch_source,
                          std::move(window_info), std::move(callback));
}

void ExtensionAppsBase::Uninstall(const std::string& app_id,
                                  apps::mojom::UninstallSource uninstall_source,
                                  bool clear_site_data,
                                  bool report_abuse) {
  // TODO(crbug.com/1009248): We need to add the error code, which could be used
  // by ExtensionFunction, ManagementUninstallFunctionBase on the callback
  // OnExtensionUninstallDialogClosed
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionRegistry::Get(profile())->GetInstalledExtension(
          app_id);
  if (!extension.get()) {
    return;
  }

  // If the extension doesn't belong to this publisher, do nothing.
  if (!Accepts(extension.get())) {
    return;
  }

  std::u16string error;
  extensions::ExtensionSystem::Get(profile())
      ->extension_service()
      ->UninstallExtension(
          app_id, GetExtensionUninstallReason(uninstall_source), &error);

  if (!report_abuse) {
    UMA_HISTOGRAM_ENUMERATION(
        "Extensions.UninstallDialogAction",
        extensions::ExtensionUninstallDialog::CLOSE_ACTION_UNINSTALL,
        extensions::ExtensionUninstallDialog::CLOSE_ACTION_LAST);
    return;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "Extensions.UninstallDialogAction",
      extensions::ExtensionUninstallDialog::
          CLOSE_ACTION_UNINSTALL_AND_CHECKBOX_CHECKED,
      extensions::ExtensionUninstallDialog::CLOSE_ACTION_LAST);

  // If the extension specifies a custom uninstall page via
  // chrome.runtime.setUninstallURL, then at uninstallation its uninstall
  // page opens. To ensure that the CWS Report Abuse page is the active
  // tab at uninstallation, navigates to the url to report abuse.
  constexpr char kReferrerId[] = "chrome-remove-extension-dialog";
  NavigateParams params(
      profile(), extension_urls::GetWebstoreReportAbuseUrl(app_id, kReferrerId),
      ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
}

void ExtensionAppsBase::OpenNativeSettings(const std::string& app_id) {
  const auto* extension = MaybeGetExtension(app_id);
  if (!extension) {
    return;
  }

  if (extension->is_hosted_app()) {
    chrome::ShowSiteSettings(
        profile_, extensions::AppLaunchInfo::GetFullLaunchURL(extension));

  } else if (extensions::ui_util::ShouldDisplayInExtensionSettings(
                 *extension)) {
    Browser* browser = chrome::FindTabbedBrowser(profile_, false);
    if (!browser) {
      browser = Browser::Create(Browser::CreateParams(profile_, true));
    }

    chrome::ShowExtensions(browser, extension->id());
  }
}

void ExtensionAppsBase::OnExtensionLastLaunchTimeChanged(
    const std::string& app_id,
    const base::Time& last_launch_time) {
  const auto* extension = MaybeGetExtension(app_id);
  if (!extension) {
    return;
  }

  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kExtension;
  app->app_id = extension->id();
  app->last_launch_time = last_launch_time;

  Publish(std::move(app), subscribers_);
}

void ExtensionAppsBase::OnExtensionPrefsWillBeDestroyed(
    extensions::ExtensionPrefs* prefs) {
  DCHECK(prefs_observation_.IsObservingSource(prefs));
  prefs_observation_.Reset();
}

void ExtensionAppsBase::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  if (!Accepts(extension)) {
    return;
  }

  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kExtension;
  app->app_id = extension->id();
  app->readiness = apps::mojom::Readiness::kReady;
  app->name = extension->name();
  app->install_reason = GetInstallReason(profile_, extension);
  app->install_source =
      app->install_reason == apps::mojom::InstallReason::kSystem
          ? apps::mojom::InstallSource::kSystem
          : apps::mojom::InstallSource::kChromeWebStore;
  Publish(std::move(app), subscribers_);
}

void ExtensionAppsBase::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  if (!Accepts(extension)) {
    return;
  }

  apps::mojom::Readiness readiness = apps::mojom::Readiness::kUnknown;

  switch (reason) {
    case extensions::UnloadedExtensionReason::DISABLE:
      readiness = apps::mojom::Readiness::kDisabledByUser;
      break;
    case extensions::UnloadedExtensionReason::BLOCKLIST:
      readiness = apps::mojom::Readiness::kDisabledByBlocklist;
      break;
    case extensions::UnloadedExtensionReason::TERMINATE:
      readiness = apps::mojom::Readiness::kTerminated;
      break;
    case extensions::UnloadedExtensionReason::UNINSTALL:
      // App readiness will be updated by OnExtensionUninstalled(). We defer to
      // that method to ensure the correct kUninstalledBy* enum is set.
      return;
    default:
      return;
  }

  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kExtension;
  app->app_id = extension->id();
  app->readiness = readiness;
  Publish(std::move(app), subscribers_);
}

void ExtensionAppsBase::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    bool is_update) {
  // If the extension doesn't belong to this publisher, do nothing.
  if (!Accepts(extension)) {
    return;
  }

  // TODO(crbug.com/826982): Does the is_update case need to be handled
  // differently? E.g. by only passing through fields that have changed.
  Publish(Convert(extension, apps::mojom::Readiness::kReady), subscribers_);
}

bool ExtensionAppsBase::RunExtensionEnableFlow(const std::string& app_id,
                                               base::OnceClosure callback) {
  if (extensions::util::IsAppLaunchableWithoutEnabling(app_id, profile_)) {
    return false;
  }

  if (enable_flow_map_.find(app_id) == enable_flow_map_.end()) {
    enable_flow_map_[app_id] =
        std::make_unique<ExtensionAppsEnableFlow>(profile_, app_id);
  }

  enable_flow_map_[app_id]->Run(
      base::BindOnce(&ExtensionAppsBase::ExtensionEnableFlowFinished,
                     weak_factory_.GetWeakPtr(), std::move(callback), app_id));
  return true;
}

void ExtensionAppsBase::ExtensionEnableFlowFinished(base::OnceClosure callback,
                                                    const std::string& app_id,
                                                    bool enabled) {
  // If the extension was not enabled, we intentionally drop the callback on the
  // floor and do nothing with it.
  if (enabled) {
    std::move(callback).Run();
  }
  enable_flow_map_.erase(app_id);
}

// static
bool ExtensionAppsBase::ShouldShow(const extensions::Extension* extension,
                                   Profile* profile) {
  if (!profile) {
    return false;
  }

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  const std::string& app_id = extension->id();
  // These three extension sets are the same three consulted by Connect.
  // Importantly, it will exclude previously installed but currently
  // uninstalled extensions.
  return registry->enabled_extensions().Contains(app_id) ||
         registry->disabled_extensions().Contains(app_id) ||
         registry->terminated_extensions().Contains(app_id);
}

void ExtensionAppsBase::PopulateIntentFilters(
    const absl::optional<GURL>& app_scope,
    std::vector<mojom::IntentFilterPtr>* target) {
  if (app_scope != absl::nullopt) {
    target->push_back(
        apps_util::CreateIntentFilterForUrlScope(app_scope.value()));
  }
}

void ExtensionAppsBase::ConvertVector(
    const extensions::ExtensionSet& extensions,
    apps::mojom::Readiness readiness,
    std::vector<apps::mojom::AppPtr>* apps_out) {
  for (const auto& extension : extensions) {
    if (Accepts(extension.get())) {
      apps_out->push_back(Convert(extension.get(), readiness));
    }
  }
}

void ExtensionAppsBase::ExtensionWasEnabled(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::IntentPtr intent,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::WindowInfoPtr window_info,
    CallbackWrapper wrapper) {
  LaunchAppWithIntent(app_id, event_flags, std::move(intent),
                      std::move(launch_source), std::move(window_info),
                      std::move(wrapper.callback));
}

}  // namespace apps
