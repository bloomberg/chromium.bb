// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_event_router.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/api/tabs/tabs_windows_api.h"
#include "chrome/browser/extensions/api/tabs/windows_event_router.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/features/feature.h"
#include "third_party/blink/public/common/page/page_zoom.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;
using content::WebContents;
using zoom::ZoomController;

namespace extensions {

namespace {

bool WillDispatchTabUpdatedEvent(
    WebContents* contents,
    const std::set<std::string> changed_property_names,
    content::BrowserContext* browser_context,
    Feature::Context target_context,
    const Extension* extension,
    Event* event,
    const base::DictionaryValue* listener_filter) {
  ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
      ExtensionTabUtil::GetScrubTabBehavior(extension, target_context,
                                            contents);
  std::unique_ptr<api::tabs::Tab> tab_object =
      ExtensionTabUtil::CreateTabObject(contents, scrub_tab_behavior,
                                        extension);

  std::unique_ptr<base::DictionaryValue> tab_value = tab_object->ToValue();

  auto changed_properties = std::make_unique<base::DictionaryValue>();
  const base::Value* value = nullptr;
  for (const auto& property : changed_property_names) {
    if (tab_value->Get(property, &value))
      changed_properties->Set(property,
                              std::make_unique<base::Value>(value->Clone()));
  }

  event->event_args->Set(1, std::move(changed_properties));
  event->event_args->Set(2, std::move(tab_value));
  return true;
}

bool WillDispatchTabCreatedEvent(WebContents* contents,
                                 bool active,
                                 content::BrowserContext* browser_context,
                                 Feature::Context target_context,
                                 const Extension* extension,
                                 Event* event,
                                 const base::DictionaryValue* listener_filter) {
  event->event_args->Clear();
  ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
      ExtensionTabUtil::GetScrubTabBehavior(extension, target_context,
                                            contents);
  std::unique_ptr<base::DictionaryValue> tab_value =
      ExtensionTabUtil::CreateTabObject(contents, scrub_tab_behavior, extension)
          ->ToValue();
  tab_value->SetBoolean(tabs_constants::kSelectedKey, active);
  tab_value->SetBoolean(tabs_constants::kActiveKey, active);
  event->event_args->Append(std::move(tab_value));
  return true;
}

}  // namespace

TabsEventRouter::TabEntry::TabEntry(TabsEventRouter* router,
                                    content::WebContents* contents)
    : WebContentsObserver(contents),
      complete_waiting_on_load_(false),
      was_audible_(false),
      was_muted_(contents->IsAudioMuted()),
      router_(router) {
  auto* audible_helper = RecentlyAudibleHelper::FromWebContents(contents);
  was_audible_ = audible_helper->WasRecentlyAudible();
}

std::set<std::string> TabsEventRouter::TabEntry::UpdateLoadState() {
  // The tab may go in & out of loading (for instance if iframes navigate).
  // We only want to respond to the first change from loading to !loading after
  // the NavigationEntryCommitted() was fired.
  if (!complete_waiting_on_load_ || web_contents()->IsLoading()) {
    return std::set<std::string>();
  }

  // Send 'status' of tab change. Expecting 'complete' is fired.
  complete_waiting_on_load_ = false;
  std::set<std::string> changed_property_names;
  changed_property_names.insert(tabs_constants::kStatusKey);
  return changed_property_names;
}

bool TabsEventRouter::TabEntry::SetAudible(bool new_val) {
  if (was_audible_ == new_val)
    return false;
  was_audible_ = new_val;
  return true;
}

bool TabsEventRouter::TabEntry::SetMuted(bool new_val) {
  if (was_muted_ == new_val)
    return false;
  was_muted_ = new_val;
  return true;
}

void TabsEventRouter::TabEntry::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  // Send 'status' of tab change. Expecting 'loading' is fired.
  complete_waiting_on_load_ = true;
  std::set<std::string> changed_property_names;
  changed_property_names.insert(tabs_constants::kStatusKey);

  if (web_contents()->GetURL() != url_) {
    url_ = web_contents()->GetURL();
    changed_property_names.insert(tabs_constants::kUrlKey);
  }

  router_->TabUpdated(this, std::move(changed_property_names));
}

void TabsEventRouter::TabEntry::TitleWasSet(content::NavigationEntry* entry) {
  std::set<std::string> changed_property_names;
  changed_property_names.insert(tabs_constants::kTitleKey);
  router_->TabUpdated(this, std::move(changed_property_names));
}

void TabsEventRouter::TabEntry::WebContentsDestroyed() {
  // This is necessary because it's possible for tabs to be created, detached
  // and then destroyed without ever having been re-attached and closed. This
  // happens in the case of a devtools WebContents that is opened in window,
  // docked, then closed.
  // Warning: |this| will be deleted after this call.
  router_->UnregisterForTabNotifications(web_contents());
}

TabsEventRouter::TabsEventRouter(Profile* profile)
    : profile_(profile), browser_tab_strip_tracker_(this, this, this) {
  DCHECK(!profile->IsOffTheRecord());

  browser_tab_strip_tracker_.Init();

  tab_manager_scoped_observer_.Add(g_browser_process->GetTabManager());
}

TabsEventRouter::~TabsEventRouter() {
}

bool TabsEventRouter::ShouldTrackBrowser(Browser* browser) {
  return profile_->IsSameProfile(browser->profile()) &&
         ExtensionTabUtil::BrowserSupportsTabs(browser);
}

void TabsEventRouter::OnBrowserSetLastActive(Browser* browser) {
  TabsWindowsAPI* tabs_window_api = TabsWindowsAPI::Get(profile_);
  if (tabs_window_api) {
    tabs_window_api->windows_event_router()->OnActiveWindowChanged(
        browser ? browser->extension_window_controller() : NULL);
  }
}

void TabsEventRouter::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  switch (change.type()) {
    case TabStripModelChange::kInserted: {
      for (const auto& contents : change.GetInsert()->contents) {
        DispatchTabInsertedAt(tab_strip_model, contents.contents,
                              contents.index,
                              selection.new_contents == contents.contents);
      }
      break;
    }
    case TabStripModelChange::kRemoved: {
      const bool will_be_deleted = change.GetRemove()->will_be_deleted;
      for (const auto& contents : change.GetRemove()->contents) {
        if (will_be_deleted) {
          DispatchTabClosingAt(tab_strip_model, contents.contents,
                               contents.index);
        }

        DispatchTabDetachedAt(contents.contents, contents.index,
                              selection.old_contents == contents.contents);
      }
      break;
    }
    case TabStripModelChange::kMoved: {
      auto* move = change.GetMove();
      DispatchTabMoved(move->contents, move->from_index, move->to_index);
      break;
    }
    case TabStripModelChange::kReplaced: {
      auto* replace = change.GetReplace();
      DispatchTabReplacedAt(replace->old_contents, replace->new_contents,
                            replace->index);
      break;
    }
    case TabStripModelChange::kSelectionOnly:
      break;
  }

  if (tab_strip_model->empty())
    return;

  if (selection.active_tab_changed()) {
    DispatchActiveTabChanged(selection.old_contents, selection.new_contents,
                             selection.new_model.active(), selection.reason);
  }

  if (selection.selection_changed()) {
    DispatchTabSelectionChanged(tab_strip_model, selection.old_model);
  }
}

void TabsEventRouter::TabChangedAt(WebContents* contents,
                                   int index,
                                   TabChangeType change_type) {
  TabEntry* entry = GetTabEntry(contents);
  // TabClosingAt() may have already removed the entry for |contents| even
  // though the tab has not yet been detached.
  if (entry)
    TabUpdated(entry, entry->UpdateLoadState());
}

void TabsEventRouter::TabPinnedStateChanged(TabStripModel* tab_strip_model,
                                            WebContents* contents,
                                            int index) {
  std::set<std::string> changed_property_names;
  changed_property_names.insert(tabs_constants::kPinnedKey);
  DispatchTabUpdatedEvent(contents, std::move(changed_property_names));
}

void TabsEventRouter::OnZoomChanged(
    const ZoomController::ZoomChangedEventData& data) {
  DCHECK(data.web_contents);
  int tab_id = ExtensionTabUtil::GetTabId(data.web_contents);
  if (tab_id < 0)
    return;

  // Prepare the zoom change information.
  api::tabs::OnZoomChange::ZoomChangeInfo zoom_change_info;
  zoom_change_info.tab_id = tab_id;
  zoom_change_info.old_zoom_factor =
      blink::PageZoomLevelToZoomFactor(data.old_zoom_level);
  zoom_change_info.new_zoom_factor =
      blink::PageZoomLevelToZoomFactor(data.new_zoom_level);
  ZoomModeToZoomSettings(data.zoom_mode, &zoom_change_info.zoom_settings);

  // Dispatch the |onZoomChange| event.
  Profile* profile =
      Profile::FromBrowserContext(data.web_contents->GetBrowserContext());
  DispatchEvent(profile, events::TABS_ON_ZOOM_CHANGE,
                api::tabs::OnZoomChange::kEventName,
                api::tabs::OnZoomChange::Create(zoom_change_info),
                EventRouter::USER_GESTURE_UNKNOWN);
}

void TabsEventRouter::OnFaviconUpdated(
    favicon::FaviconDriver* favicon_driver,
    NotificationIconType notification_icon_type,
    const GURL& icon_url,
    bool icon_url_changed,
    const gfx::Image& image) {
  if (notification_icon_type == NON_TOUCH_16_DIP && icon_url_changed) {
    favicon::ContentFaviconDriver* content_favicon_driver =
        static_cast<favicon::ContentFaviconDriver*>(favicon_driver);
    FaviconUrlUpdated(content_favicon_driver->web_contents());
  }
}

void TabsEventRouter::OnDiscardedStateChange(
    WebContents* contents,
    ::mojom::LifecycleUnitDiscardReason reason,
    bool is_discarded) {
  std::set<std::string> changed_property_names;
  changed_property_names.insert(tabs_constants::kDiscardedKey);
  DispatchTabUpdatedEvent(contents, std::move(changed_property_names));
}

void TabsEventRouter::OnAutoDiscardableStateChange(WebContents* contents,
                                                   bool is_auto_discardable) {
  std::set<std::string> changed_property_names;
  changed_property_names.insert(tabs_constants::kAutoDiscardableKey);
  DispatchTabUpdatedEvent(contents, std::move(changed_property_names));
}

void TabsEventRouter::DispatchTabInsertedAt(TabStripModel* tab_strip_model,
                                            WebContents* contents,
                                            int index,
                                            bool active) {
  if (!GetTabEntry(contents)) {
    // We've never seen this tab, send create event as long as we're not in the
    // constructor.
    if (browser_tab_strip_tracker_.is_processing_initial_browsers())
      RegisterForTabNotifications(contents);
    else
      TabCreatedAt(contents, index, active);
    return;
  }

  int tab_id = ExtensionTabUtil::GetTabId(contents);
  std::unique_ptr<base::ListValue> args(new base::ListValue);
  args->AppendInteger(tab_id);

  std::unique_ptr<base::DictionaryValue> object_args(
      new base::DictionaryValue());
  object_args->Set(
      tabs_constants::kNewWindowIdKey,
      std::make_unique<Value>(ExtensionTabUtil::GetWindowIdOfTab(contents)));
  object_args->Set(tabs_constants::kNewPositionKey,
                   std::make_unique<Value>(index));
  args->Append(std::move(object_args));

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  DispatchEvent(profile, events::TABS_ON_ATTACHED,
                api::tabs::OnAttached::kEventName, std::move(args),
                EventRouter::USER_GESTURE_UNKNOWN);
}

void TabsEventRouter::DispatchTabClosingAt(TabStripModel* tab_strip_model,
                                           WebContents* contents,
                                           int index) {
  int tab_id = ExtensionTabUtil::GetTabId(contents);

  std::unique_ptr<base::ListValue> args(new base::ListValue);
  args->AppendInteger(tab_id);

  std::unique_ptr<base::DictionaryValue> object_args(
      new base::DictionaryValue());
  object_args->SetInteger(tabs_constants::kWindowIdKey,
                          ExtensionTabUtil::GetWindowIdOfTab(contents));
  object_args->SetBoolean(tabs_constants::kWindowClosing,
                          tab_strip_model->closing_all());
  args->Append(std::move(object_args));

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  DispatchEvent(profile, events::TABS_ON_REMOVED,
                api::tabs::OnRemoved::kEventName, std::move(args),
                EventRouter::USER_GESTURE_UNKNOWN);

  UnregisterForTabNotifications(contents);
}

void TabsEventRouter::DispatchTabDetachedAt(WebContents* contents,
                                            int index,
                                            bool was_active) {
  if (!GetTabEntry(contents)) {
    // The tab was removed. Don't send detach event.
    return;
  }

  std::unique_ptr<base::ListValue> args(new base::ListValue);
  args->AppendInteger(ExtensionTabUtil::GetTabId(contents));

  std::unique_ptr<base::DictionaryValue> object_args(
      new base::DictionaryValue());
  object_args->Set(
      tabs_constants::kOldWindowIdKey,
      std::make_unique<Value>(ExtensionTabUtil::GetWindowIdOfTab(contents)));
  object_args->Set(tabs_constants::kOldPositionKey,
                   std::make_unique<Value>(index));
  args->Append(std::move(object_args));

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  DispatchEvent(profile, events::TABS_ON_DETACHED,
                api::tabs::OnDetached::kEventName, std::move(args),
                EventRouter::USER_GESTURE_UNKNOWN);
}

void TabsEventRouter::DispatchActiveTabChanged(WebContents* old_contents,
                                               WebContents* new_contents,
                                               int index,
                                               int reason) {
  auto args = std::make_unique<base::ListValue>();
  int tab_id = ExtensionTabUtil::GetTabId(new_contents);
  args->AppendInteger(tab_id);

  auto object_args = std::make_unique<base::DictionaryValue>();
  object_args->Set(tabs_constants::kWindowIdKey,
                   std::make_unique<Value>(
                       ExtensionTabUtil::GetWindowIdOfTab(new_contents)));
  args->Append(object_args->CreateDeepCopy());

  // The onActivated event replaced onActiveChanged and onSelectionChanged. The
  // deprecated events take two arguments: tabId, {windowId}.
  Profile* profile =
      Profile::FromBrowserContext(new_contents->GetBrowserContext());
  EventRouter::UserGestureState gesture =
      reason & CHANGE_REASON_USER_GESTURE
      ? EventRouter::USER_GESTURE_ENABLED
      : EventRouter::USER_GESTURE_NOT_ENABLED;
  DispatchEvent(profile, events::TABS_ON_SELECTION_CHANGED,
                api::tabs::OnSelectionChanged::kEventName,
                args->CreateDeepCopy(), gesture);
  DispatchEvent(profile, events::TABS_ON_ACTIVE_CHANGED,
                api::tabs::OnActiveChanged::kEventName, std::move(args),
                gesture);

  // The onActivated event takes one argument: {windowId, tabId}.
  auto on_activated_args = std::make_unique<base::ListValue>();
  object_args->Set(tabs_constants::kTabIdKey, std::make_unique<Value>(tab_id));
  on_activated_args->Append(std::move(object_args));
  DispatchEvent(profile, events::TABS_ON_ACTIVATED,
                api::tabs::OnActivated::kEventName,
                std::move(on_activated_args), gesture);
}

void TabsEventRouter::DispatchTabSelectionChanged(
    TabStripModel* tab_strip_model,
    const ui::ListSelectionModel& old_model) {
  ui::ListSelectionModel::SelectedIndices new_selection =
      tab_strip_model->selection_model().selected_indices();
  std::unique_ptr<base::ListValue> all_tabs(new base::ListValue);

  for (size_t i = 0; i < new_selection.size(); ++i) {
    int index = new_selection[i];
    WebContents* contents = tab_strip_model->GetWebContentsAt(index);
    if (!contents)
      break;
    int tab_id = ExtensionTabUtil::GetTabId(contents);
    all_tabs->AppendInteger(tab_id);
  }

  std::unique_ptr<base::ListValue> args(new base::ListValue);
  std::unique_ptr<base::DictionaryValue> select_info(new base::DictionaryValue);

  select_info->Set(
      tabs_constants::kWindowIdKey,
      std::make_unique<Value>(
          ExtensionTabUtil::GetWindowIdOfTabStripModel(tab_strip_model)));

  select_info->Set(tabs_constants::kTabIdsKey, std::move(all_tabs));
  args->Append(std::move(select_info));

  // The onHighlighted event replaced onHighlightChanged.
  Profile* profile = tab_strip_model->profile();
  DispatchEvent(profile, events::TABS_ON_HIGHLIGHT_CHANGED,
                api::tabs::OnHighlightChanged::kEventName,
                std::unique_ptr<base::ListValue>(args->DeepCopy()),
                EventRouter::USER_GESTURE_UNKNOWN);
  DispatchEvent(profile, events::TABS_ON_HIGHLIGHTED,
                api::tabs::OnHighlighted::kEventName, std::move(args),
                EventRouter::USER_GESTURE_UNKNOWN);
}

void TabsEventRouter::DispatchTabMoved(WebContents* contents,
                                       int from_index,
                                       int to_index) {
  std::unique_ptr<base::ListValue> args(new base::ListValue);
  args->AppendInteger(ExtensionTabUtil::GetTabId(contents));

  std::unique_ptr<base::DictionaryValue> object_args(
      new base::DictionaryValue());
  object_args->Set(
      tabs_constants::kWindowIdKey,
      std::make_unique<Value>(ExtensionTabUtil::GetWindowIdOfTab(contents)));
  object_args->Set(tabs_constants::kFromIndexKey,
                   std::make_unique<Value>(from_index));
  object_args->Set(tabs_constants::kToIndexKey,
                   std::make_unique<Value>(to_index));
  args->Append(std::move(object_args));

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  DispatchEvent(profile, events::TABS_ON_MOVED, api::tabs::OnMoved::kEventName,
                std::move(args), EventRouter::USER_GESTURE_UNKNOWN);
}

void TabsEventRouter::DispatchTabReplacedAt(WebContents* old_contents,
                                            WebContents* new_contents,
                                            int index) {
  // Notify listeners that the next tabs closing or being added are due to
  // WebContents being swapped.
  const int new_tab_id = ExtensionTabUtil::GetTabId(new_contents);
  const int old_tab_id = ExtensionTabUtil::GetTabId(old_contents);
  std::unique_ptr<base::ListValue> args(new base::ListValue);
  args->AppendInteger(new_tab_id);
  args->AppendInteger(old_tab_id);

  DispatchEvent(Profile::FromBrowserContext(new_contents->GetBrowserContext()),
                events::TABS_ON_REPLACED, api::tabs::OnReplaced::kEventName,
                std::move(args), EventRouter::USER_GESTURE_UNKNOWN);

  UnregisterForTabNotifications(old_contents);

  if (!GetTabEntry(new_contents))
    RegisterForTabNotifications(new_contents);
}

void TabsEventRouter::TabCreatedAt(WebContents* contents,
                                   int index,
                                   bool active) {
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  std::unique_ptr<base::ListValue> args(new base::ListValue);
  auto event = std::make_unique<Event>(events::TABS_ON_CREATED,
                                       api::tabs::OnCreated::kEventName,
                                       std::move(args), profile);
  event->user_gesture = EventRouter::USER_GESTURE_NOT_ENABLED;
  event->will_dispatch_callback =
      base::BindRepeating(&WillDispatchTabCreatedEvent, contents, active);
  EventRouter::Get(profile)->BroadcastEvent(std::move(event));

  RegisterForTabNotifications(contents);
}

void TabsEventRouter::TabUpdated(TabEntry* entry,
                                 std::set<std::string> changed_property_names) {
  auto* audible_helper =
      RecentlyAudibleHelper::FromWebContents(entry->web_contents());
  bool audible = audible_helper->WasRecentlyAudible();
  if (entry->SetAudible(audible)) {
    changed_property_names.insert(tabs_constants::kAudibleKey);
  }

  bool muted = entry->web_contents()->IsAudioMuted();
  if (entry->SetMuted(muted)) {
    changed_property_names.insert(tabs_constants::kMutedInfoKey);
  }

  if (!changed_property_names.empty()) {
    DispatchTabUpdatedEvent(entry->web_contents(),
                            std::move(changed_property_names));
  }
}

void TabsEventRouter::FaviconUrlUpdated(WebContents* contents) {
  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  if (!entry || !entry->GetFavicon().valid)
    return;
  std::set<std::string> changed_property_names;
  changed_property_names.insert(tabs_constants::kFaviconUrlKey);
  DispatchTabUpdatedEvent(contents, std::move(changed_property_names));
}

void TabsEventRouter::DispatchEvent(
    Profile* profile,
    events::HistogramValue histogram_value,
    const std::string& event_name,
    std::unique_ptr<base::ListValue> args,
    EventRouter::UserGestureState user_gesture) {
  EventRouter* event_router = EventRouter::Get(profile);
  if (!profile_->IsSameProfile(profile) || !event_router)
    return;

  auto event = std::make_unique<Event>(histogram_value, event_name,
                                       std::move(args), profile);
  event->user_gesture = user_gesture;
  event_router->BroadcastEvent(std::move(event));
}

void TabsEventRouter::DispatchTabUpdatedEvent(
    WebContents* contents,
    const std::set<std::string> changed_property_names) {
  DCHECK(!changed_property_names.empty());
  DCHECK(contents);

  // The state of the tab (as seen from the extension point of view) has
  // changed.  Send a notification to the extension.
  std::unique_ptr<base::ListValue> args_base(new base::ListValue);

  // First arg: The id of the tab that changed.
  args_base->AppendInteger(ExtensionTabUtil::GetTabId(contents));

  // Second arg: An object containing the changes to the tab state.  Filled in
  // by WillDispatchTabUpdatedEvent as a copy of changed_properties, if the
  // extension has the tabs permission.

  // Third arg: An object containing the state of the tab. Filled in by
  // WillDispatchTabUpdatedEvent.
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());

  auto event = std::make_unique<Event>(events::TABS_ON_UPDATED,
                                       api::tabs::OnUpdated::kEventName,
                                       std::move(args_base), profile);
  event->user_gesture = EventRouter::USER_GESTURE_NOT_ENABLED;
  event->will_dispatch_callback =
      base::BindRepeating(&WillDispatchTabUpdatedEvent, contents,
                          std::move(changed_property_names));
  EventRouter::Get(profile)->BroadcastEvent(std::move(event));
}

void TabsEventRouter::RegisterForTabNotifications(WebContents* contents) {
  favicon_scoped_observer_.Add(
      favicon::ContentFaviconDriver::FromWebContents(contents));

  ZoomController::FromWebContents(contents)->AddObserver(this);

  int tab_id = ExtensionTabUtil::GetTabId(contents);
  DCHECK(tab_entries_.find(tab_id) == tab_entries_.end());
  tab_entries_[tab_id] = std::make_unique<TabEntry>(this, contents);
}

void TabsEventRouter::UnregisterForTabNotifications(WebContents* contents) {
  favicon_scoped_observer_.Remove(
      favicon::ContentFaviconDriver::FromWebContents(contents));

  ZoomController::FromWebContents(contents)->RemoveObserver(this);

  int tab_id = ExtensionTabUtil::GetTabId(contents);
  int removed_count = tab_entries_.erase(tab_id);
  DCHECK_GT(removed_count, 0);
}

TabsEventRouter::TabEntry* TabsEventRouter::GetTabEntry(WebContents* contents) {
  const auto it = tab_entries_.find(ExtensionTabUtil::GetTabId(contents));

  return it == tab_entries_.end() ? nullptr : it->second.get();
}

}  // namespace extensions
