// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/multi_profile_browser_status_monitor.h"

#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/window_properties.h"
#include "base/containers/contains.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "ui/aura/window.h"

MultiProfileBrowserStatusMonitor::MultiProfileBrowserStatusMonitor(
    ChromeShelfController* shelf_controller)
    : BrowserStatusMonitor(shelf_controller),
      shelf_controller_(shelf_controller) {}

MultiProfileBrowserStatusMonitor::~MultiProfileBrowserStatusMonitor() {}

void MultiProfileBrowserStatusMonitor::ActiveUserChanged(
    const std::string& user_email) {
  // Handle windowed apps.
  for (AppList::iterator it = app_list_.begin(); it != app_list_.end(); ++it) {
    bool owned = multi_user_util::IsProfileFromActiveUser((*it)->profile());
    bool shown = IsV1AppInShelf(*it);
    if (owned && !shown)
      ConnectV1AppToLauncher(*it);
    else if (!owned && shown)
      DisconnectV1AppFromLauncher(*it);
  }

  // Handle apps in browser tabs: Add the new applications.
  BrowserList* browser_list = BrowserList::GetInstance();

  // Remove old (tabbed V1) applications.
  for (Browser* browser : *browser_list) {
    if (browser->is_type_normal() &&
        !multi_user_util::IsProfileFromActiveUser(browser->profile())) {
      for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
        shelf_controller_->UpdateAppState(
            browser->tab_strip_model()->GetWebContentsAt(i), true /*remove*/);
      }
    }
  }

  // Handle apps in browser tabs: Add new (tabbed V1) applications.
  for (Browser* browser : *browser_list) {
    if (browser->is_type_normal() &&
        multi_user_util::IsProfileFromActiveUser(browser->profile())) {
      for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
        shelf_controller_->UpdateAppState(
            browser->tab_strip_model()->GetWebContentsAt(i), false /*remove*/);
      }
    }
  }

  // Update the browser state since some of the removals / adds above might have
  // had an impact on the browser item.
  UpdateBrowserItemState();
}

void MultiProfileBrowserStatusMonitor::AddV1AppToShelf(Browser* browser) {
  DCHECK(browser->deprecated_is_app());
  DCHECK(!base::Contains(app_list_, browser));
  app_list_.push_back(browser);
  if (multi_user_util::IsProfileFromActiveUser(browser->profile())) {
    BrowserStatusMonitor::AddV1AppToShelf(browser);
  }
}

void MultiProfileBrowserStatusMonitor::RemoveV1AppFromShelf(Browser* browser) {
  DCHECK(browser->deprecated_is_app());
  AppList::iterator it = std::find(app_list_.begin(), app_list_.end(), browser);
  DCHECK(it != app_list_.end());
  app_list_.erase(it);
  if (multi_user_util::IsProfileFromActiveUser(browser->profile())) {
    BrowserStatusMonitor::RemoveV1AppFromShelf(browser);
  }
}

void MultiProfileBrowserStatusMonitor::ConnectV1AppToLauncher(
    Browser* browser) {
  // Adding a V1 app to the launcher consists of two actions: Add the browser
  // (launcher item) and add the content (launcher item status).
  BrowserStatusMonitor::AddV1AppToShelf(browser);

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (web_contents)
    shelf_controller_->UpdateAppState(web_contents, false /*remove*/);
}

void MultiProfileBrowserStatusMonitor::DisconnectV1AppFromLauncher(
    Browser* browser) {
  // Removing a V1 app from the launcher requires to remove the content and
  // the launcher item.
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (web_contents)
    shelf_controller_->UpdateAppState(web_contents, true /*remove*/);

  BrowserStatusMonitor::RemoveV1AppFromShelf(browser);
}
