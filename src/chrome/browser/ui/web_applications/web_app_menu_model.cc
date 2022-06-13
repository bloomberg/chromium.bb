// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_menu_model.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/strings/grit/components_strings.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "ui/base/accelerators/menu_label_accelerator_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/frame/desks/move_to_desks_menu_delegate.h"
#include "chromeos/ui/frame/desks/move_to_desks_menu_model.h"
#include "ui/views/widget/widget.h"
#endif

constexpr int WebAppMenuModel::kUninstallAppCommandId;
constexpr int WebAppMenuModel::kExtensionsMenuCommandId;

WebAppMenuModel::WebAppMenuModel(ui::AcceleratorProvider* provider,
                                 Browser* browser)
    : AppMenuModel(provider, browser) {}

WebAppMenuModel::~WebAppMenuModel() {}

bool WebAppMenuModel::IsCommandIdEnabled(int command_id) const {
  switch (command_id) {
    case kUninstallAppCommandId:
      return browser()->app_controller()->CanUserUninstall();
    case kExtensionsMenuCommandId:
      return base::FeatureList::IsEnabled(
                 features::kDesktopPWAsElidedExtensionsMenu) &&
             browser()->window()->GetExtensionsContainer() &&
             browser()->window()->GetExtensionsContainer()->HasAnyExtensions();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case chromeos::MoveToDesksMenuModel::kMenuCommandId:
      return chromeos::MoveToDesksMenuDelegate::ShouldShowMoveToDesksMenu(
          browser()->window()->GetNativeWindow());
#endif
    default:
      return AppMenuModel::IsCommandIdEnabled(command_id);
  }
}

bool WebAppMenuModel::IsCommandIdVisible(int command_id) const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (command_id == chromeos::MoveToDesksMenuModel::kMenuCommandId) {
    return chromeos::MoveToDesksMenuDelegate::ShouldShowMoveToDesksMenu(
        browser()->window()->GetNativeWindow());
  }
#endif
  return AppMenuModel::IsCommandIdVisible(command_id);
}

void WebAppMenuModel::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case kUninstallAppCommandId:
      LogMenuAction(MENU_ACTION_UNINSTALL_APP);
      browser()->app_controller()->Uninstall(
          webapps::WebappUninstallSource::kAppMenu);
      break;
    case kExtensionsMenuCommandId:
      browser()->window()->GetExtensionsContainer()->ToggleExtensionsMenu();
      break;
    default:
      AppMenuModel::ExecuteCommand(command_id, event_flags);
      break;
  }
}

void WebAppMenuModel::Build() {
  AddItemWithStringId(IDC_WEB_APP_MENU_APP_INFO,
                      IDS_APP_CONTEXT_MENU_SHOW_INFO);
  int app_info_index = GetItemCount() - 1;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  if (web_contents) {
    SetMinorText(app_info_index, web_app::AppBrowserController::FormatUrlOrigin(
                                     web_contents->GetVisibleURL()));
  }
  SetMinorIcon(app_info_index,
               ui::ImageModel::FromVectorIcon(
                   browser()->location_bar_model()->GetVectorIcon()));

  AddSeparator(ui::NORMAL_SEPARATOR);

  if (IsCommandIdEnabled(kExtensionsMenuCommandId))
    AddItemWithStringId(kExtensionsMenuCommandId, IDS_SHOW_EXTENSIONS);
  AddItemWithStringId(IDC_COPY_URL, IDS_COPY_URL);
  AddItemWithStringId(IDC_OPEN_IN_CHROME, IDS_OPEN_IN_CHROME);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (chromeos::MoveToDesksMenuDelegate::ShouldShowMoveToDesksMenu(
          browser()->window()->GetNativeWindow())) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    move_to_desks_submenu_ = std::make_unique<chromeos::MoveToDesksMenuModel>(
        std::make_unique<chromeos::MoveToDesksMenuDelegate>(
            views::Widget::GetWidgetForNativeWindow(
                browser()->window()->GetNativeWindow())));
    AddSubMenuWithStringId(chromeos::MoveToDesksMenuModel::kMenuCommandId,
                           IDS_MOVE_TO_DESKS_MENU,
                           move_to_desks_submenu_.get());
  }
#endif

// Chrome OS's app list is prominent enough to not need a separate uninstall
// option in the app menu.
#if !defined(OS_CHROMEOS)
  DCHECK(browser()->app_controller());
  if (browser()->app_controller()->IsInstalled()) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddItem(kUninstallAppCommandId,
            l10n_util::GetStringFUTF16(
                IDS_UNINSTALL_FROM_OS_LAUNCH_SURFACE,
                ui::EscapeMenuLabelAmpersands(
                    browser()->app_controller()->GetAppShortName())));
  }
#endif  // !defined(OS_CHROMEOS)
  AddSeparator(ui::LOWER_SEPARATOR);

  CreateZoomMenu();
  AddSeparator(ui::UPPER_SEPARATOR);
  AddItemWithStringId(IDC_PRINT, IDS_PRINT);
  AddItemWithStringId(IDC_FIND, IDS_FIND);
  if (media_router::MediaRouterEnabled(browser()->profile()))
    AddItemWithStringId(IDC_ROUTE_MEDIA, IDS_MEDIA_ROUTER_MENU_ITEM_TITLE);
  AddSeparator(ui::LOWER_SEPARATOR);
  CreateCutCopyPasteMenu();
}

void WebAppMenuModel::LogMenuAction(AppMenuAction action_id) {
  AppMenuModel::LogMenuAction(action_id);
  UMA_HISTOGRAM_ENUMERATION("HostedAppFrame.WrenchMenu.MenuAction", action_id,
                            LIMIT_MENU_ACTION);
}
