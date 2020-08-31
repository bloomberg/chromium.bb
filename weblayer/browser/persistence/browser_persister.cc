// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/persistence/browser_persister.h"

#include <stddef.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/command_storage_manager.h"
#include "components/sessions/core/session_command.h"
#include "components/sessions/core/session_constants.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/restore_type.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "weblayer/browser/browser_impl.h"
#include "weblayer/browser/persistence/browser_persistence_common.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/tab_impl.h"

using sessions::ContentSerializedNavigationBuilder;
using sessions::SerializedNavigationEntry;

namespace weblayer {
namespace {

int GetIndexOfTab(BrowserImpl* browser, Tab* tab) {
  const std::vector<Tab*>& tabs = browser->GetTabs();
  auto iter = std::find(tabs.begin(), tabs.end(), tab);
  DCHECK(iter != tabs.end());
  return static_cast<int>(iter - tabs.begin());
}

}  // namespace

// Every kWritesPerReset commands triggers recreating the file.
constexpr int kWritesPerReset = 250;

// BrowserPersister
// -------------------------------------------------------------

BrowserPersister::BrowserPersister(const base::FilePath& path,
                                   BrowserImpl* browser,
                                   const std::vector<uint8_t>& decryption_key)
    : browser_(browser),
      browser_session_id_(SessionID::NewUnique()),
      command_storage_manager_(
          std::make_unique<sessions::CommandStorageManager>(
              path,
              this,
              browser->profile()->GetBrowserContext()->IsOffTheRecord())),
      rebuild_on_next_save_(false),
      crypto_key_(decryption_key) {
  browser_->AddObserver(this);
  command_storage_manager_->ScheduleGetCurrentSessionCommands(
      base::BindOnce(&BrowserPersister::OnGotCurrentSessionCommands,
                     base::Unretained(this)),
      decryption_key, &cancelable_task_tracker_);
}

BrowserPersister::~BrowserPersister() {
  SaveIfNecessary();
  browser_->RemoveObserver(this);
}

void BrowserPersister::SaveIfNecessary() {
  if (command_storage_manager_->HasPendingSave())
    command_storage_manager_->Save();
}

const std::vector<uint8_t>& BrowserPersister::GetCryptoKey() const {
  return crypto_key_;
}

bool BrowserPersister::ShouldUseDelayedSave() {
  return true;
}

void BrowserPersister::OnWillSaveCommands() {
  if (!rebuild_on_next_save_)
    return;

  rebuild_on_next_save_ = false;
  command_storage_manager_->set_pending_reset(true);
  command_storage_manager_->ClearPendingCommands();
  tab_to_available_range_.clear();
  BuildCommandsForBrowser();
}

void BrowserPersister::OnGeneratedNewCryptoKey(
    const std::vector<uint8_t>& key) {
  crypto_key_ = key;
}

void BrowserPersister::OnTabAdded(Tab* tab) {
  content::WebContents* web_contents =
      static_cast<TabImpl*>(tab)->web_contents();
  auto* tab_helper = sessions::SessionTabHelper::FromWebContents(web_contents);
  DCHECK(tab_helper);
  tab_helper->SetWindowID(browser_session_id_);

  // Record the association between the SessionStorageNamespace and the
  // tab.
  content::SessionStorageNamespace* session_storage_namespace =
      web_contents->GetController().GetDefaultSessionStorageNamespace();
  session_storage_namespace->SetShouldPersist(true);

  if (rebuild_on_next_save_)
    return;

  int index = GetIndexOfTab(browser_, tab);
  BuildCommandsForTab(static_cast<TabImpl*>(tab), index);
  const std::vector<Tab*>& tabs = browser_->GetTabs();
  for (int i = index + 1; i < static_cast<int>(tabs.size()); ++i) {
    ScheduleCommand(sessions::CreateSetTabIndexInWindowCommand(
        GetSessionIDForTab(tabs[i]), i));
  }
}

void BrowserPersister::OnTabRemoved(Tab* tab, bool active_tab_changed) {
  // Allow the associated sessionStorage to get deleted; it won't be needed
  // in the session restore.
  content::WebContents* web_contents =
      static_cast<TabImpl*>(tab)->web_contents();
  content::SessionStorageNamespace* session_storage_namespace =
      web_contents->GetController().GetDefaultSessionStorageNamespace();
  session_storage_namespace->SetShouldPersist(false);

  if (rebuild_on_next_save_)
    return;

  ScheduleCommand(sessions::CreateTabClosedCommand(GetSessionIDForTab(tab)));
  const std::vector<Tab*>& tabs = browser_->GetTabs();
  for (size_t i = 0; i < tabs.size(); ++i) {
    ScheduleCommand(sessions::CreateSetTabIndexInWindowCommand(
        GetSessionIDForTab(tabs[i]), i));
  }
  auto i = tab_to_available_range_.find(GetSessionIDForTab(tab));
  if (i != tab_to_available_range_.end())
    tab_to_available_range_.erase(i);
}

void BrowserPersister::OnActiveTabChanged(Tab* tab) {
  if (rebuild_on_next_save_)
    return;

  const int index = tab == nullptr ? -1 : GetIndexOfTab(browser_, tab);
  ScheduleCommand(sessions::CreateSetSelectedTabInWindowCommand(
      browser_session_id_, index));
}

void BrowserPersister::SetTabUserAgentOverride(
    const SessionID& window_id,
    const SessionID& tab_id,
    const sessions::SerializedUserAgentOverride& user_agent_override) {
  if (rebuild_on_next_save_)
    return;

  ScheduleCommand(sessions::CreateSetTabUserAgentOverrideCommand(
      tab_id, user_agent_override));
}

void BrowserPersister::SetSelectedNavigationIndex(const SessionID& window_id,
                                                  const SessionID& tab_id,
                                                  int index) {
  if (rebuild_on_next_save_)
    return;

  if (tab_to_available_range_.find(tab_id) != tab_to_available_range_.end()) {
    if (index < tab_to_available_range_[tab_id].first ||
        index > tab_to_available_range_[tab_id].second) {
      // The new index is outside the range of what we've archived, schedule
      // a reset.
      ScheduleRebuildOnNextSave();
      return;
    }
  }
  ScheduleCommand(
      sessions::CreateSetSelectedNavigationIndexCommand(tab_id, index));
}

void BrowserPersister::UpdateTabNavigation(
    const SessionID& window_id,
    const SessionID& tab_id,
    const SerializedNavigationEntry& navigation) {
  if (rebuild_on_next_save_)
    return;

  if (tab_to_available_range_.find(tab_id) != tab_to_available_range_.end()) {
    std::pair<int, int>& range = tab_to_available_range_[tab_id];
    range.first = std::min(navigation.index(), range.first);
    range.second = std::max(navigation.index(), range.second);
  }
  ScheduleCommand(CreateUpdateTabNavigationCommand(tab_id, navigation));
}

void BrowserPersister::TabNavigationPathPruned(const SessionID& window_id,
                                               const SessionID& tab_id,
                                               int index,
                                               int count) {
  if (rebuild_on_next_save_)
    return;

  DCHECK_GE(index, 0);
  DCHECK_GT(count, 0);

  // Update the range of available indices.
  if (tab_to_available_range_.find(tab_id) != tab_to_available_range_.end()) {
    std::pair<int, int>& range = tab_to_available_range_[tab_id];

    // if both range.first and range.second are also deleted.
    if (range.second >= index && range.second < index + count &&
        range.first >= index && range.first < index + count) {
      range.first = range.second = 0;
    } else {
      // Update range.first
      if (range.first >= index + count)
        range.first = range.first - count;
      else if (range.first >= index && range.first < index + count)
        range.first = index;

      // Update range.second
      if (range.second >= index + count)
        range.second = std::max(range.first, range.second - count);
      else if (range.second >= index && range.second < index + count)
        range.second = std::max(range.first, index - 1);
    }
  }

  return ScheduleCommand(
      sessions::CreateTabNavigationPathPrunedCommand(tab_id, index, count));
}

void BrowserPersister::TabNavigationPathEntriesDeleted(
    const SessionID& window_id,
    const SessionID& tab_id) {
  if (rebuild_on_next_save_)
    return;

  // Multiple tabs might be affected by this deletion, so the rebuild is
  // delayed until next save.
  rebuild_on_next_save_ = true;
  command_storage_manager_->StartSaveTimer();
}

void BrowserPersister::ScheduleRebuildOnNextSave() {
  rebuild_on_next_save_ = true;
  command_storage_manager_->StartSaveTimer();
}

void BrowserPersister::OnGotCurrentSessionCommands(
    std::vector<std::unique_ptr<sessions::SessionCommand>> commands) {
  ScheduleRebuildOnNextSave();

  RestoreBrowserState(browser_, std::move(commands));
}

void BrowserPersister::BuildCommandsForTab(TabImpl* tab, int index_in_browser) {
  command_storage_manager_->AppendRebuildCommands(
      BuildCommandsForTabConfiguration(browser_session_id_, tab,
                                       index_in_browser));

  const SessionID& session_id = GetSessionIDForTab(tab);
  content::NavigationController& controller =
      tab->web_contents()->GetController();
  const int current_index = controller.GetCurrentEntryIndex();
  const int min_index =
      std::max(current_index - sessions::gMaxPersistNavigationCount, 0);
  const int max_index =
      std::min(current_index + sessions::gMaxPersistNavigationCount,
               controller.GetEntryCount());
  const int pending_index = controller.GetPendingEntryIndex();
  tab_to_available_range_[session_id] =
      std::pair<int, int>(min_index, max_index);

  for (int i = min_index; i < max_index; ++i) {
    content::NavigationEntry* entry = (i == pending_index)
                                          ? controller.GetPendingEntry()
                                          : controller.GetEntryAtIndex(i);
    DCHECK(entry);
    const SerializedNavigationEntry navigation =
        ContentSerializedNavigationBuilder::FromNavigationEntry(i, entry);
    command_storage_manager_->AppendRebuildCommand(
        CreateUpdateTabNavigationCommand(session_id, navigation));
  }
  command_storage_manager_->AppendRebuildCommand(
      sessions::CreateSetSelectedNavigationIndexCommand(session_id,
                                                        current_index));

  // Record the association between the sessionStorage namespace and the tab.
  content::SessionStorageNamespace* session_storage_namespace =
      controller.GetDefaultSessionStorageNamespace();
  ScheduleCommand(sessions::CreateSessionStorageAssociatedCommand(
      session_id, session_storage_namespace->id()));
}

void BrowserPersister::BuildCommandsForBrowser() {
  // This is necessary for BrowserPersister to restore the browser. The type is
  // effectively ignored.
  command_storage_manager_->AppendRebuildCommand(
      sessions::CreateSetWindowTypeCommand(
          browser_session_id_,
          sessions::SessionWindow::WindowType::TYPE_NORMAL));

  int active_index = -1;
  int tab_index = 0;
  for (Tab* tab : browser_->GetTabs()) {
    BuildCommandsForTab(static_cast<TabImpl*>(tab), tab_index);
    if (tab == browser_->GetActiveTab())
      active_index = tab_index;
    ++tab_index;
  }

  command_storage_manager_->AppendRebuildCommand(
      sessions::CreateSetSelectedTabInWindowCommand(browser_session_id_,
                                                    active_index));
}

void BrowserPersister::ScheduleCommand(
    std::unique_ptr<sessions::SessionCommand> command) {
  DCHECK(command);
  if (ReplacePendingCommand(command_storage_manager_.get(), &command))
    return;
  command_storage_manager_->ScheduleCommand(std::move(command));
  if (command_storage_manager_->commands_since_reset() >= kWritesPerReset)
    ScheduleRebuildOnNextSave();
}

}  // namespace weblayer
