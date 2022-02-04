// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/app_service/app_service_shelf_context_menu.h"

#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/ash/app_restore/full_restore_service.h"
#include "chrome/browser/ash/arc/app_shortcuts/arc_app_shortcuts_menu_builder.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_shelf_utils.h"
#include "chrome/browser/ash/crostini/crostini_terminal.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_context_menu_delegate.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/extension_app_utils.h"
#include "chrome/browser/ui/ash/shelf/arc_app_shelf_id.h"
#include "chrome/browser/ui/ash/shelf/browser_shortcut_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/views/crostini/crostini_app_restart_dialog.h"
#include "chrome/browser/ui/webui/settings/ash/app_management/app_management_uma.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/context_menu_params.h"
#include "extensions/browser/extension_prefs.h"
#include "ui/display/scoped_display_for_new_windows.h"
#include "ui/gfx/vector_icon_types.h"

namespace {

bool MenuItemHasLauncherContext(const extensions::MenuItem* item) {
  return item->contexts().Contains(extensions::MenuItem::LAUNCHER);
}

apps::mojom::WindowMode ConvertLaunchTypeCommandToWindowMode(int command_id) {
  switch (command_id) {
    case ash::LAUNCH_TYPE_REGULAR_TAB:
      return apps::mojom::WindowMode::kBrowser;
    case ash::LAUNCH_TYPE_WINDOW:
      return apps::mojom::WindowMode::kWindow;
    case ash::LAUNCH_TYPE_TABBED_WINDOW:
      return apps::mojom::WindowMode::kTabbedWindow;
    default:
      return apps::mojom::WindowMode::kUnknown;
  }
}

extensions::LaunchType ConvertLaunchTypeCommandToExtensionLaunchType(
    int command_id) {
  switch (command_id) {
    case ash::LAUNCH_TYPE_PINNED_TAB:
      return extensions::LAUNCH_TYPE_PINNED;
    case ash::LAUNCH_TYPE_REGULAR_TAB:
      return extensions::LAUNCH_TYPE_REGULAR;
    case ash::LAUNCH_TYPE_WINDOW:
      return extensions::LAUNCH_TYPE_WINDOW;
    case ash::LAUNCH_TYPE_FULLSCREEN:
      return extensions::LAUNCH_TYPE_FULLSCREEN;
    default:
      return extensions::LAUNCH_TYPE_INVALID;
  }
}

}  // namespace

AppServiceShelfContextMenu::AppServiceShelfContextMenu(
    ChromeShelfController* controller,
    const ash::ShelfItem* item,
    int64_t display_id)
    : ShelfContextMenu(controller, item, display_id) {
  if (crostini::IsUnmatchedCrostiniShelfAppId(item->id.app_id)) {
    // For Crostini app_id with the prefix "crostini:", set app_type as Unknown
    // to skip the ArcAppShelfId valid. App type can't be set as Crostini,
    // because the pin item should not be added for it.
    app_type_ = apps::AppType::kUnknown;
    return;
  }

  // Remove the ARC shelf group Prefix.
  const arc::ArcAppShelfId arc_shelf_id =
      arc::ArcAppShelfId::FromString(item->id.app_id);
  DCHECK(arc_shelf_id.valid());
  app_type_ = apps::ConvertMojomAppTypToAppType(
      apps::AppServiceProxyFactory::GetForProfile(controller->profile())
          ->AppRegistryCache()
          .GetAppType(arc_shelf_id.app_id()));
}

AppServiceShelfContextMenu::~AppServiceShelfContextMenu() = default;

void AppServiceShelfContextMenu::GetMenuModel(GetMenuModelCallback callback) {
  apps::AppServiceProxyFactory::GetForProfile(controller()->profile())
      ->GetMenuModel(
          item().id.app_id, apps::mojom::MenuType::kShelf, display_id(),
          base::BindOnce(&AppServiceShelfContextMenu::OnGetMenuModel,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void AppServiceShelfContextMenu::ExecuteCommand(int command_id,
                                                int event_flags) {
  // Place new windows on the same display as the context menu.
  display::ScopedDisplayForNewWindows scoped_display(display_id());
  if (ExecuteCommonCommand(command_id, event_flags))
    return;

  switch (command_id) {
    case ash::SHOW_APP_INFO:
      ShowAppInfo();
      ash::full_restore::FullRestoreService::MaybeCloseNotification(
          controller()->profile());
      break;

    case ash::MENU_NEW_WINDOW:
      if (app_type_ == apps::AppType::kCrostini) {
        ShelfContextMenu::ExecuteCommand(ash::MENU_OPEN_NEW, event_flags);
      } else if (app_type_ == apps::AppType::kStandaloneBrowser) {
        crosapi::BrowserManager::Get()->NewWindow(
            /*incongnito=*/false, /*should_trigger_session_restore=*/false);
      } else {
        ash::NewWindowDelegate::GetInstance()->NewWindow(
            /*incognito=*/false,
            /*should_trigger_session_restore=*/false);
      }
      ash::full_restore::FullRestoreService::MaybeCloseNotification(
          controller()->profile());
      break;

    case ash::MENU_NEW_INCOGNITO_WINDOW:
      if (app_type_ == apps::AppType::kStandaloneBrowser) {
        crosapi::BrowserManager::Get()->NewWindow(
            /*incognito=*/true, /*should_trigger_session_restore=*/false);
      } else {
        ash::NewWindowDelegate::GetInstance()->NewWindow(
            /*incognito=*/true,
            /*should_trigger_session_restore=*/false);
      }
      ash::full_restore::FullRestoreService::MaybeCloseNotification(
          controller()->profile());
      break;

    case ash::SHUTDOWN_GUEST_OS:
      if (item().id.app_id == crostini::kCrostiniTerminalSystemAppId) {
        crostini::CrostiniManager::GetForProfile(controller()->profile())
            ->StopVm(crostini::kCrostiniDefaultVmName, base::DoNothing());
      } else if (item().id.app_id == plugin_vm::kPluginVmShelfAppId) {
        plugin_vm::PluginVmManagerFactory::GetForProfile(
            controller()->profile())
            ->StopPluginVm(plugin_vm::kPluginVmName, /*force=*/false);
      } else {
        LOG(ERROR) << "App " << item().id.app_id
                   << " should not have a shutdown guest OS command.";
      }
      break;

    case ash::LAUNCH_TYPE_TABBED_WINDOW:
      [[fallthrough]];
    case ash::LAUNCH_TYPE_PINNED_TAB:
      [[fallthrough]];
    case ash::LAUNCH_TYPE_REGULAR_TAB:
      [[fallthrough]];
    case ash::LAUNCH_TYPE_WINDOW:
      [[fallthrough]];
    case ash::LAUNCH_TYPE_FULLSCREEN:
      SetLaunchType(command_id);
      break;

    case ash::CROSTINI_USE_LOW_DENSITY:
    case ash::CROSTINI_USE_HIGH_DENSITY: {
      auto* registry_service =
          guest_os::GuestOsRegistryServiceFactory::GetForProfile(
              controller()->profile());
      const bool scaled = command_id == ash::CROSTINI_USE_LOW_DENSITY;
      registry_service->SetAppScaled(item().id.app_id, scaled);
      if (controller()->IsOpen(item().id))
        crostini::ShowAppRestartDialog(display_id());
      return;
    }

    case ash::SETTINGS:
      if (item().id.app_id == crostini::kCrostiniTerminalSystemAppId) {
        crostini::LaunchTerminalSettings(controller()->profile(), display_id());
        ash::full_restore::FullRestoreService::MaybeCloseNotification(
            controller()->profile());
      }
      return;

    default:
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

      ShelfContextMenu::ExecuteCommand(command_id, event_flags);
  }
}

bool AppServiceShelfContextMenu::IsCommandIdChecked(int command_id) const {
  switch (app_type_) {
    case apps::AppType::kWeb:
    case apps::AppType::kSystemWeb: {
      if ((command_id >= ash::LAUNCH_TYPE_PINNED_TAB &&
           command_id <= ash::LAUNCH_TYPE_WINDOW) ||
          command_id == ash::LAUNCH_TYPE_TABBED_WINDOW) {
        auto user_window_mode = apps::mojom::WindowMode::kUnknown;
        apps::AppServiceProxyFactory::GetForProfile(controller()->profile())
            ->AppRegistryCache()
            .ForOneApp(item().id.app_id,
                       [&user_window_mode](const apps::AppUpdate& update) {
                         user_window_mode = update.WindowMode();
                       });
        return user_window_mode != apps::mojom::WindowMode::kUnknown &&
               user_window_mode ==
                   ConvertLaunchTypeCommandToWindowMode(command_id);
      }
      return ShelfContextMenu::IsCommandIdChecked(command_id);
    }
    case apps::AppType::kChromeApp:
      if (command_id >= ash::LAUNCH_TYPE_PINNED_TAB &&
          command_id <= ash::LAUNCH_TYPE_WINDOW) {
        return GetExtensionLaunchType() ==
               ConvertLaunchTypeCommandToExtensionLaunchType(command_id);
      } else if (command_id < ash::COMMAND_ID_COUNT) {
        return ShelfContextMenu::IsCommandIdChecked(command_id);
      } else {
        return (extension_menu_items_ &&
                extension_menu_items_->IsCommandIdChecked(command_id));
      }
    case apps::AppType::kArc:
      [[fallthrough]];
    case apps::AppType::kCrostini:
      [[fallthrough]];
    case apps::AppType::kBuiltIn:
      [[fallthrough]];
    case apps::AppType::kPluginVm:
      [[fallthrough]];
    case apps::AppType::kBorealis:
      [[fallthrough]];
    default:
      return ShelfContextMenu::IsCommandIdChecked(command_id);
  }
}

bool AppServiceShelfContextMenu::IsCommandIdEnabled(int command_id) const {
  if (command_id < ash::COMMAND_ID_COUNT)
    return ShelfContextMenu::IsCommandIdEnabled(command_id);
  if (extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(command_id) &&
      extension_menu_items_) {
    return extension_menu_items_->IsCommandIdEnabled(command_id);
  }
  return true;
}

void AppServiceShelfContextMenu::OnGetMenuModel(
    GetMenuModelCallback callback,
    apps::mojom::MenuItemsPtr menu_items) {
  auto menu_model = GetBaseMenuModel();
  submenu_ = std::make_unique<ui::SimpleMenuModel>(this);
  size_t index = 0;
  // Unretained is safe here because PopulateNewItemFromMojoMenuItems should
  // call GetVectorIcon synchronously.
  if (apps::PopulateNewItemFromMojoMenuItems(
          menu_items->items, menu_model.get(), submenu_.get(),
          base::BindOnce(&AppServiceShelfContextMenu::GetCommandIdVectorIcon,
                         base::Unretained(this)))) {
    ++index;
  }

  // The special rule to ensure that FilesManager's first menu item is "New
  // window".
  const bool build_extension_menu_before_pin =
      (app_type_ == apps::AppType::kChromeApp &&
       item().id.app_id == extension_misc::kFilesManagerAppId);

  if (build_extension_menu_before_pin)
    BuildExtensionAppShortcutsMenu(menu_model.get());

  // "New Window" should go above "Pin".
  if (menu_items->items.size() > index &&
      menu_items->items[index]->command_id == ash::MENU_NEW_WINDOW) {
    AddContextMenuOption(menu_model.get(), ash::MENU_NEW_WINDOW,
                         menu_items->items[index]->string_id);
    ++index;
  }

  if (ShouldAddPinMenu())
    AddPinMenu(menu_model.get());

  size_t shortcut_index = menu_items->items.size();
  for (size_t i = index; i < menu_items->items.size(); i++) {
    // For Chrome browser, add the close item before the app info item.
    if ((item().id.app_id == extension_misc::kChromeAppId ||
         item().id.app_id == extension_misc::kLacrosAppId) &&
        menu_items->items[i]->command_id == ash::SHOW_APP_INFO) {
      BuildChromeAppMenu(menu_model.get());
    }

    if (menu_items->items[i]->type == apps::mojom::MenuItemType::kCommand) {
      AddContextMenuOption(
          menu_model.get(),
          static_cast<ash::CommandId>(menu_items->items[i]->command_id),
          menu_items->items[i]->string_id);
    } else {
      // All shortcut menu items are appended at the end, so break out
      // of the loop and continue processing shortcut menu items in
      // BuildAppShortcutsMenu and BuildArcAppShortcutsMenu.
      shortcut_index = i;
      break;
    }
  }

  if (app_type_ == apps::AppType::kArc) {
    BuildArcAppShortcutsMenu(std::move(menu_items), std::move(menu_model),
                             std::move(callback), shortcut_index);
    return;
  }

  if (app_type_ == apps::AppType::kWeb ||
      app_type_ == apps::AppType::kSystemWeb ||
      app_type_ == apps::AppType::kCrostini) {
    BuildAppShortcutsMenu(std::move(menu_items), std::move(menu_model),
                          std::move(callback), shortcut_index);
    return;
  }

  if (!build_extension_menu_before_pin)
    BuildExtensionAppShortcutsMenu(menu_model.get());

  // When Crostini generates shelf id with the prefix "crostini:", AppService
  // can't generate the menu items, because the app_id doesn't match, so add the
  // menu items at UI side, based on the app running status.
  if (crostini::IsUnmatchedCrostiniShelfAppId(item().id.app_id)) {
    BuildCrostiniAppMenu(menu_model.get());
  }

  std::move(callback).Run(std::move(menu_model));
}

void AppServiceShelfContextMenu::BuildExtensionAppShortcutsMenu(
    ui::SimpleMenuModel* menu_model) {
  extension_menu_items_ = std::make_unique<extensions::ContextMenuMatcher>(
      controller()->profile(), this, menu_model,
      base::BindRepeating(MenuItemHasLauncherContext));

  int index = 0;
  extension_menu_items_->AppendExtensionItems(
      extensions::MenuItem::ExtensionKey(item().id.app_id), std::u16string(),
      &index, false /*is_action_menu*/);

  app_list::AddMenuItemIconsForSystemApps(
      item().id.app_id, menu_model, menu_model->GetItemCount() - index, index);
}

void AppServiceShelfContextMenu::BuildAppShortcutsMenu(
    apps::mojom::MenuItemsPtr menu_items,
    std::unique_ptr<ui::SimpleMenuModel> menu_model,
    GetMenuModelCallback callback,
    size_t shortcut_index) {
  app_shortcut_items_ = std::make_unique<apps::AppShortcutItems>();
  for (size_t i = shortcut_index; i < menu_items->items.size(); i++) {
    apps::PopulateItemFromMojoMenuItems(std::move(menu_items->items[i]),
                                        menu_model.get(),
                                        app_shortcut_items_.get());
  }
  std::move(callback).Run(std::move(menu_model));
}

void AppServiceShelfContextMenu::BuildArcAppShortcutsMenu(
    apps::mojom::MenuItemsPtr menu_items,
    std::unique_ptr<ui::SimpleMenuModel> menu_model,
    GetMenuModelCallback callback,
    size_t arc_shortcut_index) {
  const ArcAppListPrefs* arc_prefs =
      ArcAppListPrefs::Get(controller()->profile());
  DCHECK(arc_prefs);

  const arc::ArcAppShelfId& arc_shelf_id =
      arc::ArcAppShelfId::FromString(item().id.app_id);
  DCHECK(arc_shelf_id.valid());
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      arc_prefs->GetApp(arc_shelf_id.app_id());
  if (!app_info && !arc_shelf_id.has_shelf_group_id()) {
    LOG(ERROR) << "App " << item().id.app_id << " is not available.";
    std::move(callback).Run(std::move(menu_model));
    return;
  }

  if (arc_shelf_id.has_shelf_group_id()) {
    const bool app_is_open = controller()->IsOpen(item().id);
    if (!app_is_open && !app_info->suspended) {
      DCHECK(app_info->launchable);
      AddContextMenuOption(menu_model.get(), ash::MENU_OPEN_NEW,
                           IDS_APP_CONTEXT_MENU_ACTIVATE_ARC);
    }

    if (app_is_open) {
      AddContextMenuOption(menu_model.get(), ash::MENU_CLOSE,
                           IDS_SHELF_CONTEXT_MENU_CLOSE);
    }
  }

  BuildAppShortcutsMenu(std::move(menu_items), std::move(menu_model),
                        std::move(callback), arc_shortcut_index);
}

void AppServiceShelfContextMenu::BuildCrostiniAppMenu(
    ui::SimpleMenuModel* menu_model) {
  if (controller()->IsOpen(item().id)) {
    AddContextMenuOption(menu_model, ash::MENU_CLOSE,
                         IDS_SHELF_CONTEXT_MENU_CLOSE);
  } else {
    AddContextMenuOption(menu_model, ash::MENU_OPEN_NEW,
                         IDS_APP_CONTEXT_MENU_ACTIVATE_ARC);
  }
}

void AppServiceShelfContextMenu::BuildChromeAppMenu(
    ui::SimpleMenuModel* menu_model) {
  if (!BrowserShortcutShelfItemController::IsListOfActiveBrowserEmpty(
          controller()->shelf_model()) ||
      item().type == ash::TYPE_DIALOG || controller()->IsOpen(item().id)) {
    AddContextMenuOption(menu_model, ash::MENU_CLOSE,
                         IDS_SHELF_CONTEXT_MENU_CLOSE);
  }
}

void AppServiceShelfContextMenu::ShowAppInfo() {
  if (app_type_ == apps::AppType::kArc) {
    chrome::ShowAppManagementPage(
        controller()->profile(), item().id.app_id,
        ash::settings::AppManagementEntryPoint::kShelfContextMenuAppInfoArc);
    return;
  }

  // TODO(crbug.com/1196697): If this comes from Lacros app, it shows the
  // top "Apps" settings page. This is fallback, because Lacros app is not
  // registered. This is short term workaround to keep the relative
  // compatibility for Lacros Primary. We should figure out what should be shown
  // by this.
  controller()->DoShowAppInfoFlow(controller()->profile(), item().id.app_id);
}

void AppServiceShelfContextMenu::SetLaunchType(int command_id) {
  switch (app_type_) {
    case apps::AppType::kWeb:
    case apps::AppType::kSystemWeb: {
      // Web apps can only toggle between kWindow, kTabbed and kBrowser.
      apps::mojom::WindowMode user_window_mode =
          ConvertLaunchTypeCommandToWindowMode(command_id);
      if (user_window_mode != apps::mojom::WindowMode::kUnknown) {
        apps::AppServiceProxyFactory::GetForProfile(controller()->profile())
            ->SetWindowMode(item().id.app_id, user_window_mode);
      }
      return;
    }
    case apps::AppType::kChromeApp:
      SetExtensionLaunchType(command_id);
      return;
    case apps::AppType::kArc:
      [[fallthrough]];
    case apps::AppType::kCrostini:
      [[fallthrough]];
    case apps::AppType::kBuiltIn:
      [[fallthrough]];
    case apps::AppType::kPluginVm:
      [[fallthrough]];
    case apps::AppType::kBorealis:
      [[fallthrough]];
    default:
      return;
  }
}

void AppServiceShelfContextMenu::SetExtensionLaunchType(int command_id) {
  switch (static_cast<ash::CommandId>(command_id)) {
    case ash::LAUNCH_TYPE_PINNED_TAB:
      extensions::SetLaunchType(controller()->profile(), item().id.app_id,
                                extensions::LAUNCH_TYPE_PINNED);
      break;
    case ash::LAUNCH_TYPE_REGULAR_TAB:
      extensions::SetLaunchType(controller()->profile(), item().id.app_id,
                                extensions::LAUNCH_TYPE_REGULAR);
      break;
    case ash::LAUNCH_TYPE_WINDOW: {
      // Hosted apps can only toggle between LAUNCH_WINDOW and LAUNCH_REGULAR.
      extensions::LaunchType launch_type =
          GetExtensionLaunchType() == extensions::LAUNCH_TYPE_WINDOW
              ? extensions::LAUNCH_TYPE_REGULAR
              : extensions::LAUNCH_TYPE_WINDOW;

      extensions::SetLaunchType(controller()->profile(), item().id.app_id,
                                launch_type);
      break;
    }
    case ash::LAUNCH_TYPE_FULLSCREEN:
      extensions::SetLaunchType(controller()->profile(), item().id.app_id,
                                extensions::LAUNCH_TYPE_FULLSCREEN);
      break;
    default:
      return;
  }
}

extensions::LaunchType AppServiceShelfContextMenu::GetExtensionLaunchType()
    const {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(controller()->profile())
          ->GetExtensionById(item().id.app_id,
                             extensions::ExtensionRegistry::EVERYTHING);
  if (!extension)
    return extensions::LAUNCH_TYPE_DEFAULT;

  return extensions::GetLaunchType(
      extensions::ExtensionPrefs::Get(controller()->profile()), extension);
}

bool AppServiceShelfContextMenu::ShouldAddPinMenu() {
  switch (app_type_) {
    case apps::AppType::kArc: {
      const arc::ArcAppShelfId& arc_shelf_id =
          arc::ArcAppShelfId::FromString(item().id.app_id);
      DCHECK(arc_shelf_id.valid());
      const ArcAppListPrefs* arc_list_prefs =
          ArcAppListPrefs::Get(controller()->profile());
      DCHECK(arc_list_prefs);
      std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
          arc_list_prefs->GetApp(arc_shelf_id.app_id());
      if (!arc_shelf_id.has_shelf_group_id() && app_info->launchable)
        return true;
      return false;
    }
    case apps::AppType::kPluginVm:
    case apps::AppType::kBuiltIn: {
      bool show_in_launcher = false;
      apps::AppServiceProxyFactory::GetForProfile(controller()->profile())
          ->AppRegistryCache()
          .ForOneApp(item().id.app_id, [&show_in_launcher](
                                           const apps::AppUpdate& update) {
            if (update.ShowInLauncher() == apps::mojom::OptionalBool::kTrue)
              show_in_launcher = true;
          });
      return show_in_launcher;
    }
    case apps::AppType::kCrostini:
    case apps::AppType::kBorealis:
    case apps::AppType::kChromeApp:
    case apps::AppType::kWeb:
    case apps::AppType::kSystemWeb:
    case apps::AppType::kStandaloneBrowserChromeApp:
      return true;
    case apps::AppType::kStandaloneBrowser:
      // Lacros behaves like the Chrome browser icon and cannot be unpinned.
      return false;
    case apps::AppType::kUnknown:
      // Type kUnknown is used for "unregistered" Crostini apps, which do not
      // have a .desktop file and can only be closed, not pinned.
      return false;
    case apps::AppType::kMacOs:
    case apps::AppType::kRemote:
    case apps::AppType::kExtension:
      NOTREACHED() << "Type " << (int)app_type_
                   << " should not appear in shelf.";
      return false;
  }
}

void AppServiceShelfContextMenu::ExecutePublisherContextMenuCommand(
    int command_id) {
  DCHECK(command_id >= ash::LAUNCH_APP_SHORTCUT_FIRST &&
         command_id <= ash::LAUNCH_APP_SHORTCUT_LAST);
  size_t index = command_id - ash::LAUNCH_APP_SHORTCUT_FIRST;
  DCHECK(app_shortcut_items_);
  DCHECK_LT(index, app_shortcut_items_->size());

  apps::AppServiceProxyFactory::GetForProfile(controller()->profile())
      ->ExecuteContextMenuCommand(item().id.app_id, command_id,
                                  app_shortcut_items_->at(index).shortcut_id,
                                  display_id());
}
