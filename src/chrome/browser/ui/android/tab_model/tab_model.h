// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_H_
#define CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/android/tab_model/android_live_tab_context.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/omnibox/browser/location_bar_model_delegate.h"
#include "components/sessions/core/session_id.h"
#include "components/sync_sessions/synced_window_delegate.h"

struct NavigateParams;

namespace browser_sync {
class SyncedWindowDelegateAndroid;
}

namespace content {
class WebContents;
}

namespace sync_sessions {
class SyncedWindowDelegate;
}

class Profile;
class TabAndroid;
class TabModelObserver;

// Abstract representation of a Tab Model for Android.  Since Android does
// not use Browser/BrowserList, this is required to allow Chrome to interact
// with Android's Tabs and Tab Model.
class TabModel {
 public:
  // Various ways tabs can be launched.
  // Values must be numbered from 0 and can't have gaps.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.tab
  enum class TabLaunchType {
    // Opened from a link. Sets up a relationship between the newly created tab
    // and its parent.
    FROM_LINK,
    // Opened by an external app.
    FROM_EXTERNAL_APP,
    // Catch-all for Tabs opened by Chrome UI not covered by more specific
    // TabLaunchTypes.
    // Examples include:
    // - Tabs created by the options menu.
    // - Tabs created via the New Tab button in the tab stack overview.
    // - Tabs created via Push Notifications.
    // - Tabs opened via a keyboard shortcut.
    FROM_CHROME_UI,
    // Opened during the restoration process on startup or when merging two
    //  instances of
    // Chrome in Android N+ multi-instance mode.
    FROM_RESTORE,
    // Opened from the long press context menu. Will be brought to the
    // foreground.
    // Like FROM_CHROME_UI, but also sets up a parent/child relationship like
    // FROM_LINK.
    FROM_LONGPRESS_FOREGROUND,
    // Opened from the long press context menu. Will not be brought to the
    // foreground.
    // Like FROM_CHROME_UI, but also sets up a parent/child relationship like
    // FROM_LINK.
    FROM_LONGPRESS_BACKGROUND,
    // Changed windows by moving from one activity to another. Will be opened
    // in the foreground.
    FROM_REPARENTING,
    // Opened from a launcher shortcut.
    FROM_LAUNCHER_SHORTCUT,
    // The tab is created by CCT in the background and detached from
    // ChromeActivity.
    FROM_SPECULATIVE_BACKGROUND_CREATION,
    // Opened in the background from Browser Actions context menu.
    FROM_BROWSER_ACTIONS,
    // Opened by an external application launching a new Chrome incognito tab.
    FROM_LAUNCH_NEW_INCOGNITO_TAB,
    // Opened a non-restored tab during the startup process
    FROM_STARTUP,
    // Opened from the start surface.
    FROM_START_SURFACE,
    // Must be last.
    SIZE
  };

  // Various ways tabs can be selected.
  // Values must be numbered from 0 and can't have gaps.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.tab
  enum class TabSelectionType {
    // Selection of adjacent tab when the active tab is closed in foreground.
    FROM_CLOSE,
    // Selection of adjacent tab when the active tab is closed upon app exit.
    FROM_EXIT,
    // Selection of newly created tab (e.g. for a URL intent or NTP).
    FROM_NEW,
    // User-originated switch to existing tab or selection of main tab on app
    // startup.
    FROM_USER,
    // Must be last.
    SIZE
  };

  virtual Profile* GetProfile() const;
  virtual bool IsOffTheRecord() const;
  virtual sync_sessions::SyncedWindowDelegate* GetSyncedWindowDelegate() const;
  virtual SessionID GetSessionId() const;
  virtual sessions::LiveTabContext* GetLiveTabContext() const;

  virtual int GetTabCount() const = 0;
  virtual int GetActiveIndex() const = 0;
  virtual content::WebContents* GetActiveWebContents() const;
  virtual content::WebContents* GetWebContentsAt(int index) const = 0;
  // This will return NULL if the tab has not yet been initialized.
  virtual TabAndroid* GetTabAt(int index) const = 0;

  virtual void SetActiveIndex(int index) = 0;
  virtual void CloseTabAt(int index) = 0;

  // Used for restoring tabs from synced foreign sessions.
  virtual void CreateTab(TabAndroid* parent,
                         content::WebContents* web_contents) = 0;

  virtual void HandlePopupNavigation(TabAndroid* parent,
                                     NavigateParams* params) = 0;

  // Used by Developer Tools to create a new tab with a given URL.
  // Replaces CreateTabForTesting.
  virtual content::WebContents* CreateNewTabForDevTools(const GURL& url) = 0;

  // Return true if we are currently restoring sessions asynchronously.
  virtual bool IsSessionRestoreInProgress() const = 0;

  // Return true if this class is the currently selected in the correspond
  // tab model selector.
  virtual bool IsCurrentModel() const = 0;

  // Adds an observer to this TabModel.
  virtual void AddObserver(TabModelObserver* observer) = 0;

  // Removes an observer from this TabModel.
  virtual void RemoveObserver(TabModelObserver* observer) = 0;

 protected:
  explicit TabModel(Profile* profile, bool is_tabbed_activity);
  virtual ~TabModel();

  // Instructs the TabModel to broadcast a notification that all tabs are now
  // loaded from storage.
  void BroadcastSessionRestoreComplete();

  LocationBarModel* GetLocationBarModel();

 private:
  Profile* profile_;

  // The LiveTabContext associated with TabModel.
  // Used to restore closed tabs through the TabRestoreService.
  std::unique_ptr<AndroidLiveTabContext> live_tab_context_;

  // The SyncedWindowDelegate associated with this TabModel.
  std::unique_ptr<browser_sync::SyncedWindowDelegateAndroid>
      synced_window_delegate_;

  // Unique identifier of this TabModel for session restore. This id is only
  // unique within the current session, and is not guaranteed to be unique
  // across sessions.
  SessionID session_id_;

  DISALLOW_COPY_AND_ASSIGN(TabModel);
};

#endif  // CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_H_
