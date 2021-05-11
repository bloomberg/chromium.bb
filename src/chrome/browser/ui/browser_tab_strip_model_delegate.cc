// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_tab_strip_model_delegate.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/unload_controller.h"
#include "chrome/common/chrome_switches.h"
#include "components/sessions/content/content_live_tab.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ipc/ipc_message.h"
#include "ui/gfx/range/range.h"

namespace chrome {

////////////////////////////////////////////////////////////////////////////////
// BrowserTabStripModelDelegate, public:

BrowserTabStripModelDelegate::BrowserTabStripModelDelegate(Browser* browser)
    : browser_(browser) {}

BrowserTabStripModelDelegate::~BrowserTabStripModelDelegate() = default;

////////////////////////////////////////////////////////////////////////////////
// BrowserTabStripModelDelegate, TabStripModelDelegate implementation:

void BrowserTabStripModelDelegate::AddTabAt(
    const GURL& url,
    int index,
    bool foreground,
    base::Optional<tab_groups::TabGroupId> group) {
  chrome::AddTabAt(browser_, url, index, foreground, group);
}

Browser* BrowserTabStripModelDelegate::CreateNewStripWithContents(
    std::vector<NewStripContents> contentses,
    const gfx::Rect& window_bounds,
    bool maximize) {
  DCHECK(browser_->CanSupportWindowFeature(Browser::FEATURE_TABSTRIP));

  // Create an empty new browser window the same size as the old one.
  Browser::CreateParams params(browser_->profile(), true);
  params.initial_bounds = window_bounds;
  params.initial_show_state =
      maximize ? ui::SHOW_STATE_MAXIMIZED : ui::SHOW_STATE_NORMAL;
  Browser* browser = Browser::Create(params);
  TabStripModel* new_model = browser->tab_strip_model();

  for (size_t i = 0; i < contentses.size(); ++i) {
    NewStripContents item = std::move(contentses[i]);

    // Enforce that there is an active tab in the strip at all times by forcing
    // the first web contents to be marked as active.
    if (i == 0)
      item.add_types |= TabStripModel::ADD_ACTIVE;

    content::WebContents* raw_web_contents = item.web_contents.get();
    new_model->InsertWebContentsAt(
        static_cast<int>(i), std::move(item.web_contents), item.add_types);
    // Make sure the loading state is updated correctly, otherwise the throbber
    // won't start if the page is loading.
    // TODO(beng): find a better way of doing this.
    static_cast<content::WebContentsDelegate*>(browser)->LoadingStateChanged(
        raw_web_contents, true);
  }

  return browser;
}

void BrowserTabStripModelDelegate::WillAddWebContents(
    content::WebContents* contents) {
  TabHelpers::AttachTabHelpers(contents);

  // Make the tab show up in the task manager.
  task_manager::WebContentsTags::CreateForTabContents(contents);
}

int BrowserTabStripModelDelegate::GetDragActions() const {
  return TabStripModelDelegate::TAB_TEAROFF_ACTION |
         (browser_->tab_strip_model()->count() > 1
              ? TabStripModelDelegate::TAB_MOVE_ACTION
              : 0);
}

bool BrowserTabStripModelDelegate::CanDuplicateContentsAt(int index) {
  return CanDuplicateTabAt(browser_, index);
}

bool BrowserTabStripModelDelegate::CanHighlightTabs() {
  return browser_->window()->IsTabStripEditable();
}

void BrowserTabStripModelDelegate::DuplicateContentsAt(int index) {
  DuplicateTabAt(browser_, index);
}

void BrowserTabStripModelDelegate::MoveToExistingWindow(
    const std::vector<int>& indices,
    int browser_index) {
  size_t existing_browser_count = existing_browsers_for_menu_list_.size();
  if (static_cast<size_t>(browser_index) < existing_browser_count &&
      existing_browsers_for_menu_list_[browser_index]) {
    chrome::MoveTabsToExistingWindow(
        browser_, existing_browsers_for_menu_list_[browser_index].get(),
        indices);
  }
}

std::vector<base::string16>
BrowserTabStripModelDelegate::GetExistingWindowsForMoveMenu() {
  static constexpr int kWindowTitleForMenuMaxWidth = 400;
  std::vector<base::string16> window_titles;
  existing_browsers_for_menu_list_.clear();

  const BrowserList* browser_list = BrowserList::GetInstance();
  for (BrowserList::const_reverse_iterator it =
           browser_list->begin_last_active();
       it != browser_list->end_last_active(); ++it) {
    Browser* browser = *it;

    // We can only move into a tabbed view of the same profile, and not the same
    // window we're currently in.
    if (browser != browser_ && browser->is_type_normal() &&
        browser->profile() == browser_->profile()) {
      existing_browsers_for_menu_list_.push_back(browser->AsWeakPtr());
      window_titles.push_back(
          browser->GetWindowTitleForMaxWidth(kWindowTitleForMenuMaxWidth));
    }
  }
  return window_titles;
}

bool BrowserTabStripModelDelegate::CanMoveTabsToWindow(
    const std::vector<int>& indices) {
  return CanMoveTabsToNewWindow(browser_, indices);
}

void BrowserTabStripModelDelegate::MoveTabsToNewWindow(
    const std::vector<int>& indices) {
  // chrome:: to disambiguate the free function from this method.
  chrome::MoveTabsToNewWindow(browser_, indices);
}

void BrowserTabStripModelDelegate::MoveGroupToNewWindow(
    const tab_groups::TabGroupId& group) {
  gfx::Range range = browser_->tab_strip_model()
                         ->group_model()
                         ->GetTabGroup(group)
                         ->ListTabs();

  std::vector<int> indices;
  indices.reserve(range.length());
  for (auto i = range.start(); i < range.end(); ++i)
    indices.push_back(i);

  // chrome:: to disambiguate the free function from
  // BrowserTabStripModelDelegate::MoveTabsToNewWindow().
  chrome::MoveTabsToNewWindow(browser_, indices, group);
}

base::Optional<SessionID> BrowserTabStripModelDelegate::CreateHistoricalTab(
    content::WebContents* contents) {
  // We don't create historical tabs for incognito windows or windows without
  // profiles.
  if (!browser_->profile() || browser_->profile()->IsOffTheRecord())
    return base::nullopt;

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser_->profile());

  // We only create historical tab entries for tabbed browser windows.
  if (service && browser_->CanSupportWindowFeature(Browser::FEATURE_TABSTRIP)) {
    return service->CreateHistoricalTab(
        sessions::ContentLiveTab::GetForWebContents(contents),
        browser_->tab_strip_model()->GetIndexOfWebContents(contents));
  }
  return base::nullopt;
}

bool BrowserTabStripModelDelegate::RunUnloadListenerBeforeClosing(
    content::WebContents* contents) {
  return browser_->RunUnloadListenerBeforeClosing(contents);
}

bool BrowserTabStripModelDelegate::ShouldRunUnloadListenerBeforeClosing(
    content::WebContents* contents) {
  return browser_->ShouldRunUnloadListenerBeforeClosing(contents);
}

bool BrowserTabStripModelDelegate::ShouldDisplayFavicon(
    content::WebContents* contents) const {
  return browser_->ShouldDisplayFavicon(contents);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserTabStripModelDelegate, private:

void BrowserTabStripModelDelegate::CloseFrame() {
  browser_->window()->Close();
}

}  // namespace chrome
