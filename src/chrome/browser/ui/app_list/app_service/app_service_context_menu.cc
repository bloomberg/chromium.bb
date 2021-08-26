// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_service/app_service_context_menu.h"

#include "ash/public/cpp/app_menu_constants.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_terminal.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/full_restore/full_restore_service.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_context_menu_delegate.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/extension_app_utils.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/settings/chromeos/app_management/app_management_uma.h"
#include "chrome/grit/generated_resources.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "content/public/browser/context_menu_params.h"
#include "ui/display/scoped_display_for_new_windows.h"
#include "ui/gfx/vector_icon_types.h"

namespace {

bool MenuItemHasLauncherContext(const extensions::MenuItem* item) {
  return item->contexts().Contains(extensions::MenuItem::LAUNCHER);
}

apps::mojom::WindowMode ConvertUseLaunchTypeCommandToWindowMode(
    int command_id) {
  DCHECK(command_id >= ash::USE_LAUNCH_TYPE_COMMAND_START &&
         command_id < ash::USE_LAUNCH_TYPE_COMMAND_END);
  switch (command_id) {
    case ash::USE_LAUNCH_TYPE_REGULAR:
      return apps::mojom::WindowMode::kBrowser;
    case ash::USE_LAUNCH_TYPE_WINDOW:
      return apps::mojom::WindowMode::kWindow;
    case ash::USE_LAUNCH_TYPE_TABBED_WINDOW:
      return apps::mojom::WindowMode::kTabbedWindow;
    case ash::USE_LAUNCH_TYPE_PINNED:
    case ash::USE_LAUNCH_TYPE_FULLSCREEN:
    default:
      return apps::mojom::WindowMode::kUnknown;
  }
}

}  // namespace

AppServiceContextMenu::AppServiceContextMenu(
    app_list::AppContextMenuDelegate* delegate,
    Profile* profile,
    const std::string& app_id,
    AppListControllerDelegate* controller)
    : AppContextMenu(delegate, profile, app_id, controller) {
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppRegistryCache()
      .ForOneApp(app_id, [this](const apps::AppUpdate& update) {
        app_type_ = apps_util::IsInstalled(update.Readiness())
                        ? app_type_ = update.AppType()
                        : apps::mojom::AppType::kUnknown;
      });
}

AppServiceContextMenu::~AppServiceContextMenu() = default;

void AppServiceContextMenu::GetMenuModel(GetMenuModelCallback callback) {
  apps::AppServiceProxyChromeOs* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  if (proxy->AppRegistryCache().GetAppType(app_id()) ==
      apps::mojom::AppType::kUnknown) {
    std::move(callback).Run(nullptr);
    return;
  }

  proxy->GetMenuModel(
      app_id(), apps::mojom::MenuType::kAppList,
      controller()->GetAppListDisplayId(),
      base::BindOnce(&AppServiceContextMenu::OnGetMenuModel,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void AppServiceContextMenu::ExecuteCommand(int command_id, int event_flags) {
  // Place new windows on the same display as the context menu.
  display::ScopedDisplayForNewWindows scoped_display(
      controller()->GetAppListDisplayId());
  switch (command_id) {
    case ash::LAUNCH_NEW:
      delegate()->ExecuteLaunchCommand(event_flags);
      ash::full_restore::FullRestoreService::MaybeCloseNotification(profile());
      break;

    case ash::SHOW_APP_INFO:
      ShowAppInfo();
      ash::full_restore::FullRestoreService::MaybeCloseNotification(profile());
      break;

    case ash::OPTIONS:
      controller()->ShowOptionsPage(profile(), app_id());
      ash::full_restore::FullRestoreService::MaybeCloseNotification(profile());
      break;

    case ash::UNINSTALL:
      controller()->UninstallApp(profile(), app_id());
      break;

    case ash::SETTINGS:
      if (app_id() == crostini::kCrostiniTerminalSystemAppId) {
        crostini::LaunchTerminalSettings(profile(),
                                         controller()->GetAppListDisplayId());
        ash::full_restore::FullRestoreService::MaybeCloseNotification(
            profile());
      }
      break;

    case ash::APP_CONTEXT_MENU_NEW_WINDOW:
    case ash::APP_CONTEXT_MENU_NEW_INCOGNITO_WINDOW: {
      const bool is_incognito =
          command_id == ash::APP_CONTEXT_MENU_NEW_INCOGNITO_WINDOW;
      if (app_type_ == apps::mojom::AppType::kStandaloneBrowser)
        crosapi::BrowserManager::Get()->NewWindow(is_incognito);
      else
        controller()->CreateNewWindow(is_incognito,
                                      /*should_trigger_session_restore=*/false);
      ash::full_restore::FullRestoreService::MaybeCloseNotification(profile());
      break;
    }
    case ash::SHUTDOWN_GUEST_OS:
      if (app_id() == crostini::kCrostiniTerminalSystemAppId) {
        crostini::CrostiniManager::GetForProfile(profile())->StopVm(
            crostini::kCrostiniDefaultVmName, base::DoNothing());
      } else if (app_id() == plugin_vm::kPluginVmShelfAppId) {
        plugin_vm::PluginVmManagerFactory::GetForProfile(profile())
            ->StopPluginVm(plugin_vm::kPluginVmName, /*force=*/false);
      } else {
        LOG(ERROR) << "App " << app_id()
                   << " should not have a shutdown guest OS command.";
      }
      break;

    default:
      if (command_id >= ash::USE_LAUNCH_TYPE_COMMAND_START &&
          command_id < ash::USE_LAUNCH_TYPE_COMMAND_END) {
        if (app_type_ == apps::mojom::AppType::kWeb &&
            command_id == ash::USE_LAUNCH_TYPE_TABBED_WINDOW) {
          apps::AppServiceProxyFactory::GetForProfile(profile())->SetWindowMode(
              app_id(), apps::mojom::WindowMode::kTabbedWindow);
          return;
        }

        SetLaunchType(command_id);
        return;
      }

      if (extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(
              command_id)) {
        extension_menu_items_->ExecuteCommand(command_id, nullptr, nullptr,
                                              content::ContextMenuParams());
        return;
      }

      if (command_id >= ash::LAUNCH_APP_SHORTCUT_FIRST &&
          command_id <= ash::LAUNCH_APP_SHORTCUT_LAST) {
        ExecutePublisherContextMenuCommand(command_id);
        return;
      }

      AppContextMenu::ExecuteCommand(command_id, event_flags);
  }
}

bool AppServiceContextMenu::IsCommandIdChecked(int command_id) const {
  switch (app_type_) {
    case apps::mojom::AppType::kWeb:
      if (command_id >= ash::USE_LAUNCH_TYPE_COMMAND_START &&
          command_id < ash::USE_LAUNCH_TYPE_COMMAND_END) {
        auto user_window_mode = apps::mojom::WindowMode::kUnknown;
        apps::AppServiceProxyFactory::GetForProfile(profile())
            ->AppRegistryCache()
            .ForOneApp(app_id(),
                       [&user_window_mode](const apps::AppUpdate& update) {
                         user_window_mode = update.WindowMode();
                       });
        return user_window_mode != apps::mojom::WindowMode::kUnknown &&
               user_window_mode ==
                   ConvertUseLaunchTypeCommandToWindowMode(command_id);
      }
      return AppContextMenu::IsCommandIdChecked(command_id);

    case apps::mojom::AppType::kExtension:
      if (command_id >= ash::USE_LAUNCH_TYPE_COMMAND_START &&
          command_id < ash::USE_LAUNCH_TYPE_COMMAND_END) {
        return static_cast<int>(
                   controller()->GetExtensionLaunchType(profile(), app_id())) +
                   ash::USE_LAUNCH_TYPE_COMMAND_START ==
               command_id;
      } else if (extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(
                     command_id)) {
        return extension_menu_items_->IsCommandIdChecked(command_id);
      }
      return AppContextMenu::IsCommandIdChecked(command_id);

    case apps::mojom::AppType::kArc:
      FALLTHROUGH;
    case apps::mojom::AppType::kCrostini:
      FALLTHROUGH;
    case apps::mojom::AppType::kBuiltIn:
      FALLTHROUGH;
    case apps::mojom::AppType::kPluginVm:
      FALLTHROUGH;
    case apps::mojom::AppType::kBorealis:
      FALLTHROUGH;
    default:
      return AppContextMenu::IsCommandIdChecked(command_id);
  }
}

bool AppServiceContextMenu::IsCommandIdEnabled(int command_id) const {
  if (extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(command_id) &&
      extension_menu_items_) {
    return extension_menu_items_->IsCommandIdEnabled(command_id);
  }
  return AppContextMenu::IsCommandIdEnabled(command_id);
}

void AppServiceContextMenu::OnGetMenuModel(
    GetMenuModelCallback callback,
    apps::mojom::MenuItemsPtr menu_items) {
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(this);
  submenu_ = std::make_unique<ui::SimpleMenuModel>(this);
  size_t index = 0;
  if (apps::PopulateNewItemFromMojoMenuItems(
          menu_items->items, menu_model.get(), submenu_.get(),
          base::BindOnce(&AppServiceContextMenu::GetMenuItemVectorIcon))) {
    index = 1;
  }

  // The special rule to ensure that FilesManager's first menu item is "New
  // window".
  const bool build_extension_menu_before_default =
      (app_type_ == apps::mojom::AppType::kExtension &&
       app_id() == extension_misc::kFilesManagerAppId);

  if (build_extension_menu_before_default)
    BuildExtensionAppShortcutsMenu(menu_model.get());

  // Create default items for non-Remote apps.
  if (app_id() != extension_misc::kChromeAppId &&
      app_id() != extension_misc::kLacrosAppId &&
      app_type_ != apps::mojom::AppType::kUnknown &&
      app_type_ != apps::mojom::AppType::kRemote) {
    app_list::AppContextMenu::BuildMenu(menu_model.get());
  }

  if (!build_extension_menu_before_default)
    BuildExtensionAppShortcutsMenu(menu_model.get());

  app_shortcut_items_ = std::make_unique<apps::AppShortcutItems>();
  for (size_t i = index; i < menu_items->items.size(); i++) {
    if (menu_items->items[i]->type == apps::mojom::MenuItemType::kCommand) {
      AddContextMenuOption(
          menu_model.get(),
          static_cast<ash::CommandId>(menu_items->items[i]->command_id),
          menu_items->items[i]->string_id);
    } else {
      apps::PopulateItemFromMojoMenuItems(std::move(menu_items->items[i]),
                                          menu_model.get(),
                                          app_shortcut_items_.get());
    }
  }

  std::move(callback).Run(std::move(menu_model));
}

void AppServiceContextMenu::BuildExtensionAppShortcutsMenu(
    ui::SimpleMenuModel* menu_model) {
  extension_menu_items_ = std::make_unique<extensions::ContextMenuMatcher>(
      profile(), this, menu_model,
      base::BindRepeating(MenuItemHasLauncherContext));

  // Assign unique IDs to commands added by the app itself.
  int index = ash::USE_LAUNCH_TYPE_COMMAND_END;
  extension_menu_items_->AppendExtensionItems(
      extensions::MenuItem::ExtensionKey(app_id()), std::u16string(), &index,
      false /*is_action_menu*/);

  const int appended_count = index - ash::USE_LAUNCH_TYPE_COMMAND_END;
  app_list::AddMenuItemIconsForSystemApps(
      app_id(), menu_model, menu_model->GetItemCount() - appended_count,
      appended_count);
}

void AppServiceContextMenu::ShowAppInfo() {
  if (app_type_ == apps::mojom::AppType::kArc) {
    chrome::ShowAppManagementPage(
        profile(), app_id(),
        AppManagementEntryPoint::kAppListContextMenuAppInfoArc);
    return;
  }

  controller()->DoShowAppInfoFlow(profile(), app_id());
}

void AppServiceContextMenu::SetLaunchType(int command_id) {
  switch (app_type_) {
    case apps::mojom::AppType::kWeb: {
      // Web apps can only toggle between kWindow and kBrowser.
      apps::mojom::WindowMode user_window_mode =
          ConvertUseLaunchTypeCommandToWindowMode(command_id);
      if (user_window_mode != apps::mojom::WindowMode::kUnknown) {
        apps::AppServiceProxyFactory::GetForProfile(profile())->SetWindowMode(
            app_id(), user_window_mode);
      }
      return;
    }
    case apps::mojom::AppType::kExtension: {
      // Hosted apps can only toggle between LAUNCH_TYPE_WINDOW and
      // LAUNCH_TYPE_REGULAR.
      extensions::LaunchType launch_type =
          (controller()->GetExtensionLaunchType(profile(), app_id()) ==
           extensions::LAUNCH_TYPE_WINDOW)
              ? extensions::LAUNCH_TYPE_REGULAR
              : extensions::LAUNCH_TYPE_WINDOW;
      controller()->SetExtensionLaunchType(profile(), app_id(), launch_type);
      return;
    }
    case apps::mojom::AppType::kArc:
      FALLTHROUGH;
    case apps::mojom::AppType::kCrostini:
      FALLTHROUGH;
    case apps::mojom::AppType::kBuiltIn:
      FALLTHROUGH;
    case apps::mojom::AppType::kPluginVm:
      FALLTHROUGH;
    case apps::mojom::AppType::kBorealis:
      FALLTHROUGH;
    default:
      return;
  }
}

void AppServiceContextMenu::ExecutePublisherContextMenuCommand(int command_id) {
  DCHECK(command_id >= ash::LAUNCH_APP_SHORTCUT_FIRST &&
         command_id <= ash::LAUNCH_APP_SHORTCUT_LAST);
  const size_t index = command_id - ash::LAUNCH_APP_SHORTCUT_FIRST;
  DCHECK(app_shortcut_items_);
  DCHECK_LT(index, app_shortcut_items_->size());

  apps::AppServiceProxyFactory::GetForProfile(profile())
      ->ExecuteContextMenuCommand(app_id(), command_id,
                                  app_shortcut_items_->at(index).shortcut_id,
                                  controller()->GetAppListDisplayId());
}
