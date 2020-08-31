// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_H_
#define COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/token.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sessions/core/live_tab_context.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/serialized_user_agent_override.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "components/sessions/core/sessions_export.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/base/ui_base_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"

namespace sessions {

class LiveTab;
class PlatformSpecificTabData;
class TabRestoreServiceObserver;

// TabRestoreService is responsible for maintaining the most recently closed
// tabs and windows. When a tab is closed
// TabRestoreService::CreateHistoricalTab is invoked and a Tab is created to
// represent the tab. Similarly, when a browser is closed, BrowserClosing is
// invoked and a Window is created to represent the window.
//
// To restore a tab/window from the TabRestoreService invoke RestoreEntryById
// or RestoreMostRecentEntry.
//
// To listen for changes to the set of entries managed by the TabRestoreService
// add an observer.
class SESSIONS_EXPORT TabRestoreService : public KeyedService {
 public:
  // Interface used to allow the test to provide a custom time.
  class SESSIONS_EXPORT TimeFactory {
   public:
    virtual ~TimeFactory();
    virtual base::Time TimeNow() = 0;
  };

  // The type of entry.
  enum Type {
    TAB,
    WINDOW,
  };

  struct SESSIONS_EXPORT Entry {
    virtual ~Entry();

    // Unique id for this entry. The id is guaranteed to be unique for a
    // session.
    SessionID id;

    // The type of the entry.
    const Type type;

    // The time when the window or tab was closed.
    base::Time timestamp;

    // Is this entry from the last session? This is set to true for entries that
    // were closed during the last session, and false for entries that were
    // closed during this session.
    bool from_last_session = false;

    // Estimates memory usage. By default returns 0.
    virtual size_t EstimateMemoryUsage() const;

   protected:
    explicit Entry(Type type);

   private:
    DISALLOW_COPY_AND_ASSIGN(Entry);
  };

  // Represents a previously open tab.
  // If you add a new field that can allocate memory, please also add
  // it to the EstimatedMemoryUsage() implementation.
  struct SESSIONS_EXPORT Tab : public Entry {
    Tab();
    ~Tab() override;

    // Entry:
    size_t EstimateMemoryUsage() const override;

    // The navigations.
    std::vector<SerializedNavigationEntry> navigations;

    // Index of the selected navigation in navigations.
    int current_navigation_index = -1;

    // The ID of the browser to which this tab belonged, so it can be restored
    // there. May be 0 (an invalid SessionID) when restoring an entire session.
    SessionID::id_type browser_id = 0;

    // Index within the tab strip. May be -1 for an unknown index.
    int tabstrip_index = -1;

    // True if the tab was pinned.
    bool pinned = false;

    // If non-empty gives the id of the extension for the tab.
    std::string extension_app_id;

    // The associated client data.
    std::unique_ptr<PlatformSpecificTabData> platform_data;

    // The user agent override used for the tab's navigations (if applicable).
    SerializedUserAgentOverride user_agent_override;

    // The group the tab belonged to, if any.
    base::Optional<tab_groups::TabGroupId> group;

    // The group metadata for the tab, if any.
    base::Optional<tab_groups::TabGroupVisualData> group_visual_data;
  };

  // Represents a previously open window.
  // If you add a new field that can allocate memory, please also add
  // it to the EstimatedMemoryUsage() implementation.
  struct SESSIONS_EXPORT Window : public Entry {
    Window();
    ~Window() override;

    // Entry:
    size_t EstimateMemoryUsage() const override;

    // The tabs that comprised the window, in order.
    std::vector<std::unique_ptr<Tab>> tabs;

    // Tab group data.
    std::map<tab_groups::TabGroupId, tab_groups::TabGroupVisualData> tab_groups;

    // Index of the selected tab.
    int selected_tab_index = -1;

    // If an application window, the name of the app.
    std::string app_name;

    // Where and how the window is displayed.
    gfx::Rect bounds;
    ui::WindowShowState show_state;
    std::string workspace;
  };

  typedef std::list<std::unique_ptr<Entry>> Entries;
  typedef base::RepeatingCallback<bool(const SerializedNavigationEntry& entry)>
      DeletionPredicate;

  ~TabRestoreService() override;

  // Adds/removes an observer. TabRestoreService does not take ownership of
  // the observer.
  virtual void AddObserver(TabRestoreServiceObserver* observer) = 0;
  virtual void RemoveObserver(TabRestoreServiceObserver* observer) = 0;

  // Creates a Tab to represent |live_tab| and notifies observers the list of
  // entries has changed.
  virtual void CreateHistoricalTab(LiveTab* live_tab, int index) = 0;

  // TODO(blundell): Rename and fix comment.
  // Invoked when a browser is closing. If |context| is a tabbed browser with
  // at least one tab, a Window is created, added to entries and observers are
  // notified.
  virtual void BrowserClosing(LiveTabContext* context) = 0;

  // TODO(blundell): Rename and fix comment.
  // Invoked when the browser is done closing.
  virtual void BrowserClosed(LiveTabContext* context) = 0;

  // Removes all entries from the list and notifies observers the list
  // of tabs has changed.
  virtual void ClearEntries() = 0;

  // Removes all SerializedNavigationEntries matching |predicate| and notifies
  // observers the list of tabs has changed.
  virtual void DeleteNavigationEntries(const DeletionPredicate& predicate) = 0;

  // Returns the entries, ordered with most recently closed entries at the
  // front.
  virtual const Entries& entries() const = 0;

  // Restores the most recently closed entry. Does nothing if there are no
  // entries to restore. If the most recently restored entry is a tab, it is
  // added to |context|. Returns the LiveTab instances of the restored tab(s).
  virtual std::vector<LiveTab*> RestoreMostRecentEntry(
      LiveTabContext* context) = 0;

  // Removes the Tab with id |id| from the list and returns it.
  virtual std::unique_ptr<Tab> RemoveTabEntryById(SessionID id) = 0;

  // Restores an entry by id. If there is no entry with an id matching |id|,
  // this does nothing. If |context| is NULL, this creates a new window for the
  // entry. |disposition| is respected, but the attributes (tabstrip index,
  // browser window) of the tab when it was closed will be respected if
  // disposition is UNKNOWN. Returns the LiveTab instances of the restored
  // tab(s).
  virtual std::vector<LiveTab*> RestoreEntryById(
      LiveTabContext* context,
      SessionID id,
      WindowOpenDisposition disposition) = 0;

  // Loads the tabs and previous session. This does nothing if the tabs
  // from the previous session have already been loaded.
  virtual void LoadTabsFromLastSession() = 0;

  // Returns true if the tab entries have been loaded.
  virtual bool IsLoaded() const = 0;

  // Deletes the last session.
  virtual void DeleteLastSession() = 0;

  // Returns true if we're in the process of restoring some entries.
  virtual bool IsRestoring() const = 0;
};

// A class that is used to associate platform-specific data with
// TabRestoreService::Tab. See LiveTab::GetPlatformSpecificTabData().
class SESSIONS_EXPORT PlatformSpecificTabData {
 public:
  virtual ~PlatformSpecificTabData();
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_H_
