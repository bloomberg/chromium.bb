// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/app_shortcut_launcher_item_controller.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/ash/launcher/arc_playstore_shortcut_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_util.h"
#include "chrome/browser/ui/ash/launcher/launcher_controller_helper.h"
#include "chrome/browser/ui/ash/launcher/shelf_context_menu.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"

using extensions::Extension;
using extensions::ExtensionRegistry;

namespace {

// The time delta between clicks in which clicks to launch V2 apps are ignored.
const int kClickSuppressionInMS = 1000;

bool IsAppBrowser(Browser* browser) {
  return browser->is_type_app() || browser->is_type_app_popup();
}

// AppMatcher is used to determine if various WebContents instances are
// associated with a specific app. Clients should call CanMatchWebContents()
// before iterating through WebContents instances and calling
// WebContentMatchesApp().
class AppMatcher {
 public:
  AppMatcher(Profile* profile,
             const std::string& app_id,
             const URLPattern& refocus_pattern)
      : app_id_(app_id), refocus_pattern_(refocus_pattern) {
    DCHECK(profile);
    if (web_app::WebAppProviderBase* provider =
            web_app::WebAppProviderBase::GetProviderBase(profile)) {
      if (provider->registrar().IsLocallyInstalled(app_id)) {
        registrar_ = &provider->registrar();
      }
    }
    if (!registrar_)
      extension_ = GetExtensionForAppID(app_id, profile);
  }

  AppMatcher(const AppMatcher&) = delete;
  AppMatcher& operator=(const AppMatcher&) = delete;

  bool CanMatchWebContents() const { return registrar_ || extension_; }

  // Returns true if this app matches the given |web_contents|. If
  // the browser is an app browser, the application gets first checked against
  // its original URL since a windowed app might have navigated away from its
  // app domain.
  // May only be called if CanMatchWebContents() return true.
  bool WebContentMatchesApp(content::WebContents* web_contents,
                            Browser* browser) const {
    DCHECK(CanMatchWebContents());
    return extension_ ? WebContentMatchesHostedApp(web_contents, browser)
                      : WebContentMatchesWebApp(web_contents, browser);
  }

 private:
  bool WebContentMatchesHostedApp(content::WebContents* web_contents,
                                  Browser* browser) const {
    DCHECK(extension_);
    DCHECK(!registrar_);

    // If the browser is an app window, and the app name matches the extension,
    // then the contents match the app.
    if (IsAppBrowser(browser)) {
      const Extension* browser_extension =
          ExtensionRegistry::Get(browser->profile())
              ->GetExtensionById(
                  web_app::GetAppIdFromApplicationName(browser->app_name()),
                  ExtensionRegistry::EVERYTHING);
      return browser_extension == extension_;
    }

    // Apps set to launch in app windows should not match contents running in
    // tabs.
    if (extensions::LaunchesInWindow(browser->profile(), extension_))
      return false;

    // There are three ways to identify the association of a URL with this
    // extension:
    // - The refocus pattern is matched (needed for apps like drive).
    // - The extension's origin + extent gets matched.
    // - The launcher controller knows that the tab got created for this app.
    const GURL tab_url = web_contents->GetURL();
    return (
        (!refocus_pattern_.match_all_urls() &&
         refocus_pattern_.MatchesURL(tab_url)) ||
        (extension_->OverlapsWithOrigin(tab_url) &&
         extension_->web_extent().MatchesURL(tab_url)) ||
        ChromeLauncherController::instance()->IsWebContentHandledByApplication(
            web_contents, app_id_));
  }

  // Returns true if this web app matches the given |web_contents|. If
  // |deprecated_is_app| is true, the application gets first checked against its
  // original URL since a windowed app might have navigated away from its app
  // domain.
  bool WebContentMatchesWebApp(content::WebContents* web_contents,
                               Browser* browser) const {
    DCHECK(registrar_);
    DCHECK(!extension_);

    // If the browser is a web app window, and the window app id matches,
    // then the contents match the app.
    if (browser->app_controller() && browser->app_controller()->HasAppId())
      return browser->app_controller()->GetAppId() == app_id_;

    // Bookmark apps set to launch in app windows should not match contents
    // running in tabs.
    if (registrar_->GetAppUserDisplayMode(app_id_) !=
            web_app::DisplayMode::kBrowser &&
        // TODO(crbug.com/1054116): when the flag is on, we allow web
        // contents in a normal browser to match a web app. This is going to
        // be weird because GetAppMenuItems(0) and HasRunningApplications()
        // does not consider normal browsers.
        !base::FeatureList::IsEnabled(
            features::kDesktopPWAsWithoutExtensions)) {
      return false;
    }

    // There are three ways to identify the association of a URL with this
    // web app:
    // - The refocus pattern is matched (needed for apps like drive).
    // - The web app's scope gets matched.
    // - The launcher controller knows that the tab got created for this web
    // app.
    const GURL tab_url = web_contents->GetURL();
    base::Optional<GURL> app_scope = registrar_->GetAppScope(app_id_);
    DCHECK(app_scope.has_value());

    return (
        (!refocus_pattern_.match_all_urls() &&
         refocus_pattern_.MatchesURL(tab_url)) ||
        (base::StartsWith(tab_url.spec(), app_scope->spec(),
                          base::CompareCase::SENSITIVE)) ||
        ChromeLauncherController::instance()->IsWebContentHandledByApplication(
            web_contents, app_id_));
  }

  const std::string app_id_;
  const URLPattern refocus_pattern_;

  // AppMatcher is stack allocated. Pointer members below are not owned.

  // registrar_ is set when app_id_ is a web app.
  const web_app::AppRegistrar* registrar_ = nullptr;

  // extension_ is set when app_id_ is a hosted app.
  const Extension* extension_ = nullptr;
};

}  // namespace

// static
std::unique_ptr<AppShortcutLauncherItemController>
AppShortcutLauncherItemController::Create(const ash::ShelfID& shelf_id) {
  if (shelf_id.app_id == arc::kPlayStoreAppId)
    return std::make_unique<ArcPlaystoreShortcutLauncherItemController>();
  return base::WrapUnique<AppShortcutLauncherItemController>(
      new AppShortcutLauncherItemController(shelf_id));
}

AppShortcutLauncherItemController::AppShortcutLauncherItemController(
    const ash::ShelfID& shelf_id)
    : ash::ShelfItemDelegate(shelf_id) {
  BrowserList::AddObserver(this);

  // To detect V1 applications we use their domain and match them against the
  // used URL. This will also work with applications like Google Drive.
  const Extension* extension = GetExtensionForAppID(
      shelf_id.app_id, ChromeLauncherController::instance()->profile());
  // Some unit tests have no real extension.
  if (extension) {
    set_refocus_url(GURL(
        extensions::AppLaunchInfo::GetLaunchWebURL(extension).spec() + "*"));
  }
}

AppShortcutLauncherItemController::~AppShortcutLauncherItemController() {
  BrowserList::RemoveObserver(this);
}

void AppShortcutLauncherItemController::ItemSelected(
    std::unique_ptr<ui::Event> event,
    int64_t display_id,
    ash::ShelfLaunchSource source,
    ItemSelectedCallback callback) {
  // In case of a keyboard event, we were called by a hotkey. In that case we
  // activate the next item in line if an item of our list is already active.
  if (event && event->type() == ui::ET_KEY_RELEASED) {
    auto optional_action = AdvanceToNextApp();
    if (optional_action.has_value()) {
      std::move(callback).Run(optional_action.value(), {});
      return;
    }
  }

  AppMenuItems items = GetAppMenuItems(event ? event->flags() : ui::EF_NONE);
  if (items.empty()) {
    // Ideally we come here only once. After that ShellLauncherItemController
    // will take over when the shell window gets opened. However there are apps
    // which take a lot of time for pre-processing (like the files app) before
    // they open a window. Since there is currently no other way to detect if an
    // app was started we suppress any further clicks within a special time out.
    if (IsV2App() && !AllowNextLaunchAttempt()) {
      std::move(callback).Run(ash::SHELF_ACTION_NONE, std::move(items));
      return;
    }

    // LaunchApp may replace and destroy this item controller instance. Run the
    // callback first and copy the id to avoid crashes.
    std::move(callback).Run(ash::SHELF_ACTION_NEW_WINDOW_CREATED, {});
    ChromeLauncherController::instance()->LaunchApp(
        ash::ShelfID(shelf_id()), source, ui::EF_NONE, display_id);
    return;
  }

  if (items.size() == 1) {
    DCHECK(AppMenuSize() == 1);
    std::move(callback).Run(
        app_menu_cached_by_browsers_
            ? ChromeLauncherController::instance()
                  ->ActivateWindowOrMinimizeIfActive(
                      // We don't need to check nullptr here because
                      // we just called GetAppMenuItems() above to update it.
                      app_menu_browsers_[0]->window(), true)
            : ActivateContentOrMinimize(app_menu_web_contents_[0], true),
        {});
  } else {
    // Multiple items, a menu will be shown. No need to activate the most
    // recently active item.
    std::move(callback).Run(ash::SHELF_ACTION_NONE, std::move(items));
  }
}

bool AppShortcutLauncherItemController::HasRunningApplications() {
  return IsWindowedWebApp() ? !GetAppBrowsers().empty()
                            : !GetAppWebContents().empty();
}

ash::ShelfItemDelegate::AppMenuItems
AppShortcutLauncherItemController::GetAppMenuItems(int event_flags) {
  ChromeLauncherController* controller = ChromeLauncherController::instance();
  AppMenuItems items;
  auto add_menu_item = [&controller,
                        &items](content::WebContents* web_contents) {
    items.push_back({controller->GetAppMenuTitle(web_contents),
                     controller->GetAppMenuIcon(web_contents).AsImageSkia()});
  };

  if (IsWindowedWebApp() && !(event_flags & ui::EF_SHIFT_DOWN)) {
    app_menu_browsers_ = GetAppBrowsers();
    app_menu_cached_by_browsers_ = true;
    for (auto* browser : app_menu_browsers_) {
      add_menu_item(browser->tab_strip_model()->GetActiveWebContents());
    }
  } else {
    app_menu_web_contents_ = GetAppWebContents();
    app_menu_cached_by_browsers_ = false;
    for (auto* web_contents : app_menu_web_contents_) {
      add_menu_item(web_contents);
    }
  }

  return items;
}

void AppShortcutLauncherItemController::GetContextMenu(
    int64_t display_id,
    GetContextMenuCallback callback) {
  ChromeLauncherController* controller = ChromeLauncherController::instance();
  const ash::ShelfItem* item = controller->GetItem(shelf_id());
  context_menu_ = ShelfContextMenu::Create(controller, item, display_id);
  context_menu_->GetMenuModel(std::move(callback));
}

void AppShortcutLauncherItemController::ExecuteCommand(bool from_context_menu,
                                                       int64_t command_id,
                                                       int32_t event_flags,
                                                       int64_t display_id) {
  if (from_context_menu && ExecuteContextMenuCommand(command_id, event_flags))
    return;

  if (static_cast<size_t>(command_id) >= AppMenuSize()) {
    ClearAppMenu();
    return;
  }

  bool should_close =
      event_flags & (ui::EF_SHIFT_DOWN | ui::EF_MIDDLE_MOUSE_BUTTON);
  auto activate_browser = [](Browser* browser) {
    multi_user_util::MoveWindowToCurrentDesktop(
        browser->window()->GetNativeWindow());
    browser->window()->Show();
    browser->window()->Activate();
  };

  if (app_menu_cached_by_browsers_) {
    Browser* browser = app_menu_browsers_[command_id];
    if (browser) {
      if (should_close)
        browser->tab_strip_model()->CloseAllTabs();
      else
        activate_browser(browser);
    }
  } else {
    // If the web contents was destroyed while the menu was open, then the
    // invalid pointer cached in |app_menu_web_contents_| should yield a null
    // browser or kNoTab.
    content::WebContents* web_contents = app_menu_web_contents_[command_id];
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
    TabStripModel* tab_strip = browser ? browser->tab_strip_model() : nullptr;
    const int index = tab_strip ? tab_strip->GetIndexOfWebContents(web_contents)
                                : TabStripModel::kNoTab;
    if (index != TabStripModel::kNoTab) {
      if (should_close) {
        tab_strip->CloseWebContentsAt(index, TabStripModel::CLOSE_USER_GESTURE);
      } else {
        tab_strip->ActivateTabAt(index);
        activate_browser(browser);
      }
    }
  }

  ClearAppMenu();
}

void AppShortcutLauncherItemController::Close() {
  // Close all running 'programs' of this type.
  if (IsWindowedWebApp()) {
    for (Browser* browser : GetAppBrowsers())
      browser->tab_strip_model()->CloseAllTabs();
  } else {
    for (content::WebContents* item : GetAppWebContents()) {
      Browser* browser = chrome::FindBrowserWithWebContents(item);
      if (!browser ||
          !multi_user_util::IsProfileFromActiveUser(browser->profile())) {
        continue;
      }
      TabStripModel* tab_strip = browser->tab_strip_model();
      int index = tab_strip->GetIndexOfWebContents(item);
      DCHECK(index != TabStripModel::kNoTab);
      tab_strip->CloseWebContentsAt(index, TabStripModel::CLOSE_NONE);
    }
  }
}

void AppShortcutLauncherItemController::OnBrowserClosing(Browser* browser) {
  if (!app_menu_cached_by_browsers_)
    return;
  // Reset pointers to the closed browser, but leave menu indices intact.
  auto it =
      std::find(app_menu_browsers_.begin(), app_menu_browsers_.end(), browser);
  if (it != app_menu_browsers_.end())
    *it = nullptr;
}

std::vector<content::WebContents*>
AppShortcutLauncherItemController::GetAppWebContents() {
  URLPattern refocus_pattern(URLPattern::SCHEME_ALL);
  refocus_pattern.SetMatchAllURLs(true);

  if (!refocus_url_.is_empty()) {
    refocus_pattern.SetMatchAllURLs(false);
    refocus_pattern.Parse(refocus_url_.spec());
  }

  Profile* const profile = ChromeLauncherController::instance()->profile();
  AppMatcher matcher(profile, app_id(), refocus_pattern);

  std::vector<content::WebContents*> items;
  // It is possible to come here while an app gets loaded.
  if (!matcher.CanMatchWebContents())
    return items;

  for (auto* browser : *BrowserList::GetInstance()) {
    if (!multi_user_util::IsProfileFromActiveUser(browser->profile()))
      continue;
    TabStripModel* tab_strip = browser->tab_strip_model();
    for (int index = 0; index < tab_strip->count(); index++) {
      content::WebContents* web_contents = tab_strip->GetWebContentsAt(index);
      if (matcher.WebContentMatchesApp(web_contents, browser))
        items.push_back(web_contents);
    }
  }
  return items;
}

std::vector<Browser*> AppShortcutLauncherItemController::GetAppBrowsers() {
  DCHECK(IsWindowedWebApp());
  std::vector<Browser*> browsers;
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (!multi_user_util::IsProfileFromActiveUser(browser->profile()))
      continue;
    if (!IsAppBrowser(browser))
      continue;

    if (web_app::GetAppIdFromApplicationName(browser->app_name()) == app_id() &&
        browser->tab_strip_model()->GetActiveWebContents()) {
      browsers.push_back(browser);
    }
  }
  return browsers;
}

ash::ShelfAction AppShortcutLauncherItemController::ActivateContentOrMinimize(
    content::WebContents* content,
    bool allow_minimize) {
  Browser* browser = chrome::FindBrowserWithWebContents(content);
  TabStripModel* tab_strip = browser->tab_strip_model();
  int index = tab_strip->GetIndexOfWebContents(content);
  DCHECK_NE(TabStripModel::kNoTab, index);

  int old_index = tab_strip->active_index();
  if (index != old_index)
    tab_strip->ActivateTabAt(index);
  return ChromeLauncherController::instance()->ActivateWindowOrMinimizeIfActive(
      browser->window(), index == old_index && allow_minimize);
}

base::Optional<ash::ShelfAction>
AppShortcutLauncherItemController::AdvanceToNextApp() {
  Browser* browser = chrome::FindLastActive();
  // The last active browser is not necessarily the active window. The window
  // could be a v2 app or ARC app.
  if (browser && browser->window()->IsActive()) {
    if (IsWindowedWebApp()) {
      std::vector<Browser*> browsers = GetAppBrowsers();
      auto it = std::find(browsers.cbegin(), browsers.cend(), browser);
      if (it == browsers.cend()) {
        return base::nullopt;
      }
      if (browsers.size() == 1) {
        ash_util::BounceWindow(browser->window()->GetNativeWindow());
        return ash::SHELF_ACTION_NONE;
      }
      size_t index = (it - browsers.cbegin() + 1) % browsers.size();
      browsers[index]->window()->Show();
      browsers[index]->window()->Activate();
      return ash::SHELF_ACTION_WINDOW_ACTIVATED;
    } else {
      std::vector<content::WebContents*> items = GetAppWebContents();
      TabStripModel* tab_strip = browser->tab_strip_model();
      content::WebContents* active_content = tab_strip->GetActiveWebContents();
      auto it = std::find(items.cbegin(), items.cend(), active_content);
      if (it != items.cend()) {
        if (items.size() == 1) {
          ash_util::BounceWindow(browser->window()->GetNativeWindow());
          return ash::SHELF_ACTION_NONE;
        }
        size_t index = (it - items.cbegin() + 1) % items.size();
        ActivateContentOrMinimize(items[index], false);
        return ash::SHELF_ACTION_WINDOW_ACTIVATED;
      }
    }
  }
  return base::nullopt;
}

bool AppShortcutLauncherItemController::IsV2App() {
  const Extension* extension = GetExtensionForAppID(
      app_id(), ChromeLauncherController::instance()->profile());
  return extension && extension->is_platform_app();
}

bool AppShortcutLauncherItemController::AllowNextLaunchAttempt() {
  if (last_launch_attempt_.is_null() ||
      last_launch_attempt_ +
              base::TimeDelta::FromMilliseconds(kClickSuppressionInMS) <
          base::Time::Now()) {
    last_launch_attempt_ = base::Time::Now();
    return true;
  }
  return false;
}

bool AppShortcutLauncherItemController::IsWindowedWebApp() {
  if (web_app::WebAppProviderBase* provider =
          web_app::WebAppProviderBase::GetProviderBase(
              ChromeLauncherController::instance()->profile())) {
    web_app::AppRegistrar& registrar = provider->registrar();
    if (registrar.IsLocallyInstalled(app_id())) {
      return registrar.GetAppUserDisplayMode(app_id()) !=
             web_app::DisplayMode::kBrowser;
    }
  }
  return false;
}

size_t AppShortcutLauncherItemController::AppMenuSize() {
  return app_menu_cached_by_browsers_ ? app_menu_browsers_.size()
                                      : app_menu_web_contents_.size();
}

void AppShortcutLauncherItemController::ClearAppMenu() {
  app_menu_browsers_.clear();
  app_menu_web_contents_.clear();
}
