// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SHORTCUT_LAUNCHER_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SHORTCUT_LAUNCHER_ITEM_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

class Browser;
class ShelfContextMenu;

// Item controller for an app shortcut.
// If the associated app is a platform or ARC app, launching the app replaces
// this instance with an AppWindowLauncherItemController to handle the app's
// windows. Closing all associated AppWindows will replace that delegate with
// a new instance of this class (if the app is pinned to the shelf).
//
// Non-platform app types do not use AppWindows. This delegate is not replaced
// when browser windows are opened for those app types.
class AppShortcutLauncherItemController : public ash::ShelfItemDelegate,
                                          public BrowserListObserver {
 public:
  ~AppShortcutLauncherItemController() override;

  static std::unique_ptr<AppShortcutLauncherItemController> Create(
      const ash::ShelfID& shelf_id);

  // ash::ShelfItemDelegate overrides:
  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ash::ShelfLaunchSource source,
                    ItemSelectedCallback callback) override;
  AppMenuItems GetAppMenuItems(int event_flags) override;
  void GetContextMenu(int64_t display_id,
                      GetContextMenuCallback callback) override;
  void ExecuteCommand(bool from_context_menu,
                      int64_t command_id,
                      int32_t event_flags,
                      int64_t display_id) override;
  void Close() override;

  // Get the refocus url pattern, which can be used to identify this application
  // from a URL link.
  const GURL& refocus_url() const { return refocus_url_; }
  // Set the refocus url pattern. Used by unit tests.
  void set_refocus_url(const GURL& refocus_url) { refocus_url_ = refocus_url; }

  bool HasRunningApplications();

 protected:
  explicit AppShortcutLauncherItemController(const ash::ShelfID& shelf_id);

 private:
  // BrowserListObserver:
  void OnBrowserClosing(Browser* browser) override;

  std::vector<content::WebContents*> GetAppWebContents();
  std::vector<Browser*> GetAppBrowsers();

  // Activate the browser with the given |content| and show the associated tab,
  // or minimize the browser if it is already active. Returns the action
  // performed by activating the content.
  ash::ShelfAction ActivateContentOrMinimize(content::WebContents* content,
                                             bool allow_minimize);

  // If an owned item is already active, this function advances to the next item
  // (or bounce the browser if there is only one item) and returns a shelf
  // action. Otherwise, it returns nullopt.
  base::Optional<ash::ShelfAction> AdvanceToNextApp();

  // Returns true if the application is a V2 app.
  bool IsV2App();

  // Returns true if it is allowed to try starting a V2 app again.
  bool AllowNextLaunchAttempt();

  bool IsWindowedWebApp();

  size_t AppMenuSize();
  void ClearAppMenu();

  GURL refocus_url_;

  // Since V2 applications can be undetectable after launching, this timer is
  // keeping track of the last launch attempt.
  base::Time last_launch_attempt_;

  // The cached lists of open app shown in an application menu. We either cache
  // by the web contents or by the browsers, and this is indicated by the value
  // of |app_menu_cached_by_browsers_|.
  std::vector<content::WebContents*> app_menu_web_contents_;
  std::vector<Browser*> app_menu_browsers_;
  bool app_menu_cached_by_browsers_ = false;

  std::unique_ptr<ShelfContextMenu> context_menu_;

  DISALLOW_COPY_AND_ASSIGN(AppShortcutLauncherItemController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SHORTCUT_LAUNCHER_ITEM_CONTROLLER_H_
