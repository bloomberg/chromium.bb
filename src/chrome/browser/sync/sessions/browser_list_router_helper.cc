// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/browser_list_router_helper.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

namespace sync_sessions {

BrowserListRouterHelper::BrowserListRouterHelper(
    SyncSessionsWebContentsRouter* router,
    Profile* profile)
    : router_(router), profile_(profile) {
  BrowserList* browser_list = BrowserList::GetInstance();
  for (Browser* browser : *browser_list) {
    if (browser->profile() == profile_) {
      browser->tab_strip_model()->AddObserver(this);
    }
  }
  browser_list->AddObserver(this);
}

BrowserListRouterHelper::~BrowserListRouterHelper() {
  BrowserList* browser_list = BrowserList::GetInstance();
  for (Browser* browser : *browser_list) {
    if (browser->profile() == profile_) {
      browser->tab_strip_model()->RemoveObserver(this);
    }
  }

  BrowserList::GetInstance()->RemoveObserver(this);
}

void BrowserListRouterHelper::OnBrowserAdded(Browser* browser) {
  if (browser->profile() == profile_) {
    browser->tab_strip_model()->AddObserver(this);
  }
}

void BrowserListRouterHelper::OnBrowserRemoved(Browser* browser) {
  if (browser->profile() == profile_) {
    browser->tab_strip_model()->RemoveObserver(this);
  }
}

void BrowserListRouterHelper::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() != TabStripModelChange::kInserted &&
      change.type() != TabStripModelChange::kReplaced)
    return;

  for (const auto& delta : change.deltas()) {
    content::WebContents* web_contents =
        change.type() == TabStripModelChange::kInserted
            ? delta.insert.contents
            : delta.replace.new_contents;
    if (Profile::FromBrowserContext(web_contents->GetBrowserContext()) ==
        profile_) {
      router_->NotifyTabModified(web_contents, false);
    }
  }
}

}  // namespace sync_sessions
