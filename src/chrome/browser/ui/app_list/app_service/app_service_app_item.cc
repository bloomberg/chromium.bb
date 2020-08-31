// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_service/app_service_app_item.h"

#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_service/app_service_context_menu.h"
#include "chrome/browser/ui/app_list/arc/arc_app_context_menu.h"
#include "chrome/browser/ui/app_list/crostini/crostini_app_context_menu.h"
#include "chrome/browser/ui/app_list/extension_app_context_menu.h"
#include "chrome/browser/ui/app_list/web_app_context_menu.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/common/chrome_features.h"

// static
const char AppServiceAppItem::kItemType[] = "AppServiceAppItem";

// static
std::unique_ptr<app_list::AppContextMenu> AppServiceAppItem::MakeAppContextMenu(
    apps::mojom::AppType app_type,
    AppContextMenuDelegate* delegate,
    Profile* profile,
    const std::string& app_id,
    AppListControllerDelegate* controller,
    bool is_platform_app) {
  // Terminal System App uses CrostiniAppContextMenu.
  if (app_id == crostini::GetTerminalId()) {
    return std::make_unique<CrostiniAppContextMenu>(profile, app_id,
                                                    controller);
  }

  switch (app_type) {
    case apps::mojom::AppType::kUnknown:
    case apps::mojom::AppType::kBuiltIn:
    case apps::mojom::AppType::kLacros:
      return std::make_unique<app_list::AppContextMenu>(delegate, profile,
                                                        app_id, controller);

    case apps::mojom::AppType::kArc:
      return std::make_unique<ArcAppContextMenu>(delegate, profile, app_id,
                                                 controller);

    case apps::mojom::AppType::kCrostini:
      return std::make_unique<CrostiniAppContextMenu>(profile, app_id,
                                                      controller);

    case apps::mojom::AppType::kPluginVm:
      return std::make_unique<app_list::AppContextMenu>(delegate, profile,
                                                        app_id, controller);

    case apps::mojom::AppType::kWeb:
      if (base::FeatureList::IsEnabled(
              features::kDesktopPWAsWithoutExtensions)) {
        return std::make_unique<app_list::WebAppContextMenu>(
            delegate, profile, app_id, controller);
      }
      // Otherwise deliberately fall through to fallback on Bookmark Apps.
      FALLTHROUGH;

    case apps::mojom::AppType::kExtension:
      return std::make_unique<app_list::ExtensionAppContextMenu>(
          delegate, profile, app_id, controller, is_platform_app);

    case apps::mojom::AppType::kMacNative:
      NOTREACHED() << "Should not be trying to make a menu for a native app";
      return nullptr;
  }

  return nullptr;
}

AppServiceAppItem::AppServiceAppItem(
    Profile* profile,
    AppListModelUpdater* model_updater,
    const app_list::AppListSyncableService::SyncItem* sync_item,
    const apps::AppUpdate& app_update)
    : ChromeAppListItem(profile, app_update.AppId()),
      app_type_(app_update.AppType()),
      is_platform_app_(false) {
  OnAppUpdate(app_update, true);
  if (sync_item && sync_item->item_ordinal.IsValid()) {
    UpdateFromSync(sync_item);
  } else {
    SetDefaultPositionIfApplicable(model_updater);

    // Crostini apps and the Terminal System App start in the crostini folder.
    if (app_type_ == apps::mojom::AppType::kCrostini ||
        id() == crostini::GetTerminalId()) {
      DCHECK(folder_id().empty());
      SetChromeFolderId(crostini::kCrostiniFolderId);
    }
  }

  // Set model updater last to avoid being called during construction.
  set_model_updater(model_updater);
}

AppServiceAppItem::~AppServiceAppItem() = default;

void AppServiceAppItem::OnAppUpdate(const apps::AppUpdate& app_update) {
  OnAppUpdate(app_update, false);
}

void AppServiceAppItem::OnAppUpdate(const apps::AppUpdate& app_update,
                                    bool in_constructor) {
  if (in_constructor || app_update.NameChanged()) {
    SetName(app_update.Name());
  }

  if (in_constructor || app_update.IconKeyChanged()) {
    constexpr bool allow_placeholder_icon = true;
    CallLoadIcon(allow_placeholder_icon);
  }

  if (in_constructor || app_update.IsPlatformAppChanged()) {
    is_platform_app_ =
        app_update.IsPlatformApp() == apps::mojom::OptionalBool::kTrue;
  }
}

void AppServiceAppItem::Activate(int event_flags) {
  // For Crostini apps, non-platform Chrome apps, Web apps, it could be
  // selecting an existing delegate for the app, so call
  // ChromeLauncherController's ActivateApp interface. Platform apps or ARC
  // apps, Crostini apps treat activations as a launch. The app can decide
  // whether to show a new window or focus an existing window as it sees fit.
  //
  // TODO(crbug.com/1022541): Move the Chrome special case to ExtensionApps,
  // when AppService Instance feature is done.
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  if (!proxy)
    return;
  bool is_active_app = false;
  proxy->AppRegistryCache().ForOneApp(
      id(), [&is_active_app](const apps::AppUpdate& update) {
        if (update.AppType() == apps::mojom::AppType::kCrostini ||
            ((update.AppType() == apps::mojom::AppType::kExtension ||
              update.AppType() == apps::mojom::AppType::kWeb) &&
             update.IsPlatformApp() == apps::mojom::OptionalBool::kFalse)) {
          is_active_app = true;
        }
      });
  if (is_active_app) {
    ChromeLauncherController::instance()->ActivateApp(
        id(), ash::LAUNCH_FROM_APP_LIST, event_flags,
        GetController()->GetAppListDisplayId());
    return;
  }
  Launch(event_flags, apps::mojom::LaunchSource::kFromAppListGrid);
}

const char* AppServiceAppItem::GetItemType() const {
  return AppServiceAppItem::kItemType;
}

void AppServiceAppItem::GetContextMenuModel(GetMenuModelCallback callback) {
  if (base::FeatureList::IsEnabled(features::kAppServiceContextMenu)) {
    context_menu_ = std::make_unique<AppServiceContextMenu>(
        this, profile(), id(), GetController());
  } else {
    context_menu_ = MakeAppContextMenu(app_type_, this, profile(), id(),
                                       GetController(), is_platform_app_);
  }

  context_menu_->GetMenuModel(std::move(callback));
}

app_list::AppContextMenu* AppServiceAppItem::GetAppContextMenu() {
  return context_menu_.get();
}

void AppServiceAppItem::ExecuteLaunchCommand(int event_flags) {
  Launch(event_flags, apps::mojom::LaunchSource::kFromAppListGridContextMenu);

  // TODO(crbug.com/826982): drop the if, and call MaybeDismissAppList
  // unconditionally?
  if (app_type_ == apps::mojom::AppType::kArc) {
    MaybeDismissAppList();
  }
}

void AppServiceAppItem::Launch(int event_flags,
                               apps::mojom::LaunchSource launch_source) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  if (proxy) {
    proxy->Launch(id(), event_flags, launch_source,
                  GetController()->GetAppListDisplayId());
  }
}

void AppServiceAppItem::CallLoadIcon(bool allow_placeholder_icon) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  if (proxy) {
    proxy->LoadIcon(app_type_, id(),
                    apps::mojom::IconCompression::kUncompressed,
                    ash::AppListConfig::instance().grid_icon_dimension(),
                    allow_placeholder_icon,
                    base::BindOnce(&AppServiceAppItem::OnLoadIcon,
                                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void AppServiceAppItem::OnLoadIcon(apps::mojom::IconValuePtr icon_value) {
  if (icon_value->icon_compression !=
      apps::mojom::IconCompression::kUncompressed) {
    return;
  }
  SetIcon(icon_value->uncompressed);

  if (icon_value->is_placeholder_icon) {
    constexpr bool allow_placeholder_icon = false;
    CallLoadIcon(allow_placeholder_icon);
  }
}
