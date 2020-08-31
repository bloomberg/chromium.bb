// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/crostini_apps.h"

#include <utility>

#include "ash/public/cpp/app_menu_constants.h"
#include "chrome/browser/apps/app_service/dip_px_util.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_package_service.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/strings/grit/ui_strings.h"

// TODO(crbug.com/826982): the equivalent of
// CrostiniAppModelBuilder::MaybeCreateRootFolder. Does some sort of "root
// folder" abstraction belong here (on the publisher side of the App Service)
// or should we hard-code that in one particular subscriber (the App List UI)?

namespace {

bool ShouldShowDisplayDensityMenuItem(const std::string& app_id,
                                      apps::mojom::MenuType menu_type,
                                      int64_t display_id) {
  // The default terminal app is crosh in a Chrome window and it doesn't run in
  // the Crostini container so it doesn't support display density the same way.
  if (menu_type != apps::mojom::MenuType::kShelf ||
      app_id == crostini::GetTerminalId()) {
    return false;
  }

  display::Display d;
  if (!display::Screen::GetScreen()->GetDisplayWithDisplayId(display_id, &d)) {
    return true;
  }

  return d.device_scale_factor() != 1.0;
}

}  // namespace

namespace apps {

CrostiniApps::CrostiniApps(
    const mojo::Remote<apps::mojom::AppService>& app_service,
    Profile* profile)
    : profile_(profile), registry_(nullptr), crostini_enabled_(false) {
  Initialize(app_service);
}

CrostiniApps::~CrostiniApps() {
  if (registry_) {
    registry_->RemoveObserver(this);
  }
}

void CrostiniApps::ReInitializeForTesting(
    const mojo::Remote<apps::mojom::AppService>& app_service,
    Profile* profile) {
  // Some test code creates a profile (and therefore profile-linked services
  // like the App Service) before it creates the fake user that lets
  // IsCrostiniUIAllowedForProfile return true. To work around that, we issue a
  // second Initialize call.
  receiver().reset();
  profile_ = profile;
  registry_ = nullptr;
  crostini_enabled_ = false;

  Initialize(app_service);
}

void CrostiniApps::Initialize(
    const mojo::Remote<apps::mojom::AppService>& app_service) {
  DCHECK(profile_);
  if (!crostini::CrostiniFeatures::Get()->IsUIAllowed(profile_)) {
    return;
  }
  registry_ = guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile_);
  if (!registry_) {
    return;
  }
  crostini_enabled_ = crostini::CrostiniFeatures::Get()->IsEnabled(profile_);

  registry_->AddObserver(this);

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(profile_->GetPrefs());
  pref_change_registrar_->Add(
      crostini::prefs::kCrostiniEnabled,
      base::BindRepeating(&CrostiniApps::OnCrostiniEnabledChanged,
                          base::Unretained(this)));

  PublisherBase::Initialize(app_service, apps::mojom::AppType::kCrostini);
}

void CrostiniApps::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  std::vector<apps::mojom::AppPtr> apps;
  for (const auto& pair :
       registry_->GetRegisteredApps(guest_os::GuestOsRegistryService::VmType::
                                        ApplicationList_VmType_TERMINA)) {
    const std::string& app_id = pair.first;
    const guest_os::GuestOsRegistryService::Registration& registration =
        pair.second;
    apps.push_back(Convert(app_id, registration, true));
  }
  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  subscriber->OnApps(std::move(apps));
  subscribers_.Add(std::move(subscriber));
}

void CrostiniApps::LoadIcon(const std::string& app_id,
                            apps::mojom::IconKeyPtr icon_key,
                            apps::mojom::IconCompression icon_compression,
                            int32_t size_hint_in_dip,
                            bool allow_placeholder_icon,
                            LoadIconCallback callback) {
  if (icon_key) {
    if (icon_key->resource_id != apps::mojom::IconKey::kInvalidResourceId) {
      // The icon is a resource built into the Chrome OS binary.
      constexpr bool is_placeholder_icon = false;
      LoadIconFromResource(icon_compression, size_hint_in_dip,
                           icon_key->resource_id, is_placeholder_icon,
                           static_cast<IconEffects>(icon_key->icon_effects),
                           std::move(callback));
      return;
    } else {
      auto scale_factor = apps_util::GetPrimaryDisplayUIScaleFactor();

      // Try loading the icon from an on-disk cache. If that fails, fall back
      // to LoadIconFromVM.
      LoadIconFromFileWithFallback(
          icon_compression, size_hint_in_dip,
          registry_->GetIconPath(app_id, scale_factor),
          static_cast<IconEffects>(icon_key->icon_effects), std::move(callback),
          base::BindOnce(&CrostiniApps::LoadIconFromVM,
                         weak_ptr_factory_.GetWeakPtr(), app_id,
                         icon_compression, size_hint_in_dip,
                         allow_placeholder_icon, scale_factor,
                         static_cast<IconEffects>(icon_key->icon_effects)));
      return;
    }
  }

  // On failure, we still run the callback, with the zero IconValue.
  std::move(callback).Run(apps::mojom::IconValue::New());
}

void CrostiniApps::Launch(const std::string& app_id,
                          int32_t event_flags,
                          apps::mojom::LaunchSource launch_source,
                          int64_t display_id) {
  crostini::LaunchCrostiniApp(profile_, app_id, display_id);
}

void CrostiniApps::Uninstall(const std::string& app_id,
                             bool clear_site_data,
                             bool report_abuse) {
  crostini::CrostiniPackageService::GetForProfile(profile_)
      ->QueueUninstallApplication(app_id);
}

void CrostiniApps::GetMenuModel(const std::string& app_id,
                                apps::mojom::MenuType menu_type,
                                int64_t display_id,
                                GetMenuModelCallback callback) {
  apps::mojom::MenuItemsPtr menu_items = apps::mojom::MenuItems::New();

  if (menu_type == apps::mojom::MenuType::kShelf) {
    AddCommandItem(ash::MENU_NEW_WINDOW, IDS_APP_LIST_NEW_WINDOW, &menu_items);
  }

  if (crostini::IsUninstallable(profile_, app_id)) {
    AddCommandItem(ash::UNINSTALL, IDS_APP_LIST_UNINSTALL_ITEM, &menu_items);
  }

  if (app_id == crostini::GetTerminalId()) {
    if (base::FeatureList::IsEnabled(features::kTerminalSystemApp)) {
      AddCommandItem(ash::SETTINGS, IDS_INTERNAL_APP_SETTINGS, &menu_items);
    }
    if (crostini::IsCrostiniRunning(profile_)) {
      AddCommandItem(ash::SHUTDOWN_GUEST_OS,
                     IDS_CROSTINI_SHUT_DOWN_LINUX_MENU_ITEM, &menu_items);
    }
  }

  if (ShouldAddOpenItem(app_id, menu_type, profile_)) {
    AddCommandItem(ash::MENU_OPEN_NEW, IDS_APP_CONTEXT_MENU_ACTIVATE_ARC,
                   &menu_items);
  }

  if (ShouldAddCloseItem(app_id, menu_type, profile_)) {
    AddCommandItem(ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE, &menu_items);
  }

  // Offer users the ability to toggle per-application UI scaling.
  // Some apps have high-density display support and do not require scaling
  // to match the system display density, but others are density-unaware and
  // look better when scaled to match the display density.
  if (ShouldShowDisplayDensityMenuItem(app_id, menu_type, display_id)) {
    base::Optional<guest_os::GuestOsRegistryService::Registration>
        registration = registry_->GetRegistration(app_id);
    if (registration) {
      if (registration->IsScaled()) {
        AddCommandItem(ash::CROSTINI_USE_HIGH_DENSITY,
                       IDS_CROSTINI_USE_HIGH_DENSITY, &menu_items);
      } else {
        AddCommandItem(ash::CROSTINI_USE_LOW_DENSITY,
                       IDS_CROSTINI_USE_LOW_DENSITY, &menu_items);
      }
    }
  }

  std::move(callback).Run(std::move(menu_items));
}

void CrostiniApps::OnRegistryUpdated(
    guest_os::GuestOsRegistryService* registry_service,
    const std::vector<std::string>& updated_apps,
    const std::vector<std::string>& removed_apps,
    const std::vector<std::string>& inserted_apps) {
  for (const std::string& app_id : updated_apps) {
    PublishAppID(app_id, PublishAppIDType::kUpdate);
  }
  for (const std::string& app_id : removed_apps) {
    PublishAppID(app_id, PublishAppIDType::kUninstall);
  }
  for (const std::string& app_id : inserted_apps) {
    PublishAppID(app_id, PublishAppIDType::kInstall);
  }
}

void CrostiniApps::OnAppIconUpdated(const std::string& app_id,
                                    ui::ScaleFactor scale_factor) {
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kCrostini;
  app->app_id = app_id;
  app->icon_key = NewIconKey(app_id);
  Publish(std::move(app), subscribers_);
}

void CrostiniApps::OnCrostiniEnabledChanged() {
  crostini_enabled_ =
      profile_ && crostini::CrostiniFeatures::Get()->IsEnabled(profile_);
  auto show = crostini_enabled_ ? apps::mojom::OptionalBool::kTrue
                                : apps::mojom::OptionalBool::kFalse;

  // The Crostini Terminal app is a hard-coded special case. It is the entry
  // point to installing other Crostini apps.
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kCrostini;
  app->app_id = crostini::GetTerminalId();
  app->show_in_launcher = show;
  app->show_in_search = show;
  Publish(std::move(app), subscribers_);
}

void CrostiniApps::LoadIconFromVM(const std::string app_id,
                                  apps::mojom::IconCompression icon_compression,
                                  int32_t size_hint_in_dip,
                                  bool allow_placeholder_icon,
                                  ui::ScaleFactor scale_factor,
                                  IconEffects icon_effects,
                                  LoadIconCallback callback) {
  if (!allow_placeholder_icon) {
    // Treat this as failure. We still run the callback, with a nullptr to
    // indicate failure.
    std::move(callback).Run(nullptr);
    return;
  }

  // Provide a placeholder icon.
  constexpr bool is_placeholder_icon = true;
  LoadIconFromResource(icon_compression, size_hint_in_dip,
                       IDR_LOGO_CROSTINI_DEFAULT_192, is_placeholder_icon,
                       icon_effects, std::move(callback));

  // Ask the VM to load the icon (and write a cached copy to the file system).
  // The "Maybe" is because multiple requests for the same icon will be merged,
  // calling OnAppIconUpdated only once. In OnAppIconUpdated, we'll publish a
  // new IconKey, and subscribers can re-schedule new LoadIcon calls, with new
  // LoadIconCallback's, that will pick up that cached copy.
  //
  // TODO(crbug.com/826982): add a safeguard to prevent an infinite loop where
  // OnAppIconUpdated somehow doesn't write the cached icon file where we
  // expect, leading to another MaybeRequestIcon call, leading to another
  // OnAppIconUpdated call, leading to another MaybeRequestIcon call, etc.
  registry_->MaybeRequestIcon(app_id, scale_factor);
}

apps::mojom::AppPtr CrostiniApps::Convert(
    const std::string& app_id,
    const guest_os::GuestOsRegistryService::Registration& registration,
    bool new_icon_key) {
  apps::mojom::AppPtr app = PublisherBase::MakeApp(
      apps::mojom::AppType::kCrostini, app_id, apps::mojom::Readiness::kReady,
      registration.Name(), apps::mojom::InstallSource::kUser);

  const std::string& executable_file_name = registration.ExecutableFileName();
  if (!executable_file_name.empty()) {
    app->additional_search_terms.push_back(executable_file_name);
  }
  for (const std::string& keyword : registration.Keywords()) {
    app->additional_search_terms.push_back(keyword);
  }

  if (new_icon_key) {
    app->icon_key = NewIconKey(app_id);
  }

  app->last_launch_time = registration.LastLaunchTime();
  app->install_time = registration.InstallTime();

  auto show = !registration.NoDisplay() ? apps::mojom::OptionalBool::kTrue
                                        : apps::mojom::OptionalBool::kFalse;
  auto show_in_search = show;
  if (registration.is_terminal_app()) {
    show = crostini_enabled_ ? apps::mojom::OptionalBool::kTrue
                             : apps::mojom::OptionalBool::kFalse;
    // The Crostini Terminal should appear in the app search, even when
    // Crostini is not installed.
    show_in_search = apps::mojom::OptionalBool::kTrue;
  }
  app->show_in_launcher = show;
  app->show_in_search = show_in_search;
  // TODO(crbug.com/955937): Enable once Crostini apps are managed inside App
  // Management.
  app->show_in_management = apps::mojom::OptionalBool::kFalse;

  return app;
}

apps::mojom::IconKeyPtr CrostiniApps::NewIconKey(const std::string& app_id) {
  DCHECK(!app_id.empty());

  // Treat the Crostini Terminal as a special case, loading an icon defined by
  // a resource instead of asking the Crostini VM (or the cache of previous
  // responses from the Crostini VM). Presumably this is for bootstrapping: the
  // Crostini Terminal icon (the UI for enabling and installing Crostini apps)
  // should be showable even before the user has installed their first Crostini
  // app and before bringing up an Crostini VM for the first time.
  if (app_id == crostini::GetTerminalId()) {
    return apps::mojom::IconKey::New(
        apps::mojom::IconKey::kDoesNotChangeOverTime,
        IDR_LOGO_CROSTINI_TERMINAL, apps::IconEffects::kNone);
  }

  return icon_key_factory_.MakeIconKey(apps::IconEffects::kNone);
}

void CrostiniApps::PublishAppID(const std::string& app_id,
                                PublishAppIDType type) {
  if (type == PublishAppIDType::kUninstall) {
    apps::mojom::AppPtr app = apps::mojom::App::New();
    app->app_type = apps::mojom::AppType::kCrostini;
    app->app_id = app_id;
    app->readiness = apps::mojom::Readiness::kUninstalledByUser;
    Publish(std::move(app), subscribers_);
    return;
  }

  base::Optional<guest_os::GuestOsRegistryService::Registration> registration =
      registry_->GetRegistration(app_id);
  if (registration.has_value()) {
    Publish(Convert(app_id, *registration, type == PublishAppIDType::kInstall),
            subscribers_);
  }
}

}  // namespace apps
