// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_SESSION_RESTORATION_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_SESSIONS_SESSION_RESTORATION_BROWSER_AGENT_H_

#include <CoreFoundation/CoreFoundation.h>

#include <memory>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "ios/chrome/browser/main/browser_observer.h"
#include "ios/chrome/browser/main/browser_user_data.h"
#include "ios/chrome/browser/web_state_list/web_state_list_observer.h"
#include "ios/web/public/web_state_observer.h"

class AllWebStateObservationForwarder;
class ChromeBrowserState;
@class SessionWindowIOS;
@class SessionIOSFactory;
class SessionRestorationObserver;
@class SessionServiceIOS;
class WebStateList;
class WebUsageEnablerBrowserAgent;

// This class is responsible for handling requests of session restoration. It
// can be observed via SeassonRestorationObserver which it uses to notify
// observers of session restoration events. This class also automatically
// save sessions when the active webState changes, and when each web state
// completes a navigation.
class SessionRestorationBrowserAgent
    : BrowserObserver,
      public BrowserUserData<SessionRestorationBrowserAgent>,
      WebStateListObserver,
      public web::WebStateObserver {
 public:
  // Creates an SessionRestorationBrowserAgent scoped to |browser|.
  static void CreateForBrowser(Browser* browser,
                               SessionServiceIOS* session_service);

  ~SessionRestorationBrowserAgent() override;

  SessionRestorationBrowserAgent(const SessionRestorationBrowserAgent&) =
      delete;
  SessionRestorationBrowserAgent& operator=(
      const SessionRestorationBrowserAgent&) = delete;

  // Set a session identification string that will be used to locate which
  // session to restore. Must be set before restoring/saving the session.
  void SetSessionID(NSString* session_identifier);

  // Adds/Removes Observer to session restoration events.
  void AddObserver(SessionRestorationObserver* observer);
  void RemoveObserver(SessionRestorationObserver* observer);

  // Restores the |window| (for example, after a crash). If there is only one ,
  // tab showing the NTP, then this tab should be clobbered, otherwise, the tabs
  // from the restored sessions should be added at the end of the current list
  // of tabs. Returns YES if the single NTP tab is closed.
  bool RestoreSessionWindow(SessionWindowIOS* window);

  // Restores the session whose ID matches the session ID set for this agent.
  // Restoration is done via RestoreSessionWindow(), above, and the return
  // value of that method is returned.
  bool RestoreSession();

  // Persists the current list of tabs to disk, either immediately or deferred
  // based on the value of |immediately|.
  void SaveSession(const bool immediately);

  // Returns true if there is a session restoration in progress, otherwise it
  // returns false. Note that this method can be called from the UI Thread.
  // This method exists as a work around for crbug.com/763964.
  bool IsRestoringSession();

 private:
  SessionRestorationBrowserAgent(Browser* browser,
                                 SessionServiceIOS* session_service);

  friend class BrowserUserData<SessionRestorationBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  // Returns true if the current session can be saved.
  bool CanSaveSession();

  // BrowserObserver methods
  void BrowserDestroyed(Browser* browser) override;

  // WebStateListObserver methods.
  void WebStateActivatedAt(WebStateList* web_state_list,
                           web::WebState* old_web_state,
                           web::WebState* new_web_state,
                           int active_index,
                           ActiveWebStateChangeReason reason) override;
  void WillDetachWebStateAt(WebStateList* web_state_list,
                            web::WebState* web_state,
                            int index) override;
  void WebStateInsertedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index,
                          bool activating) override;
  void WebStateReplacedAt(WebStateList* web_state_list,
                          web::WebState* old_web_state,
                          web::WebState* new_web_state,
                          int index) override;
  void WebStateMoved(WebStateList* web_state_list,
                     web::WebState* web_state,
                     int from_index,
                     int to_index) override;

  // web::WebStateObserver methods.
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;

  // The service object which handles the actual saving of sessions.
  SessionServiceIOS* session_service_ = nullptr;

  // The list of web states to be saved.
  WebStateList* web_state_list_ = nullptr;

  // The web usage enabler for the web state list being restored.
  WebUsageEnablerBrowserAgent* web_enabler_ = nullptr;

  base::ObserverList<SessionRestorationObserver, true> observers_;

  ChromeBrowserState* browser_state_ = nullptr;

  // Session Factory used to create session data for saving.
  SessionIOSFactory* session_ios_factory_ = nullptr;

  // Session identifier for this agent.
  __strong NSString* session_identifier_ = nil;

  // True when session restoration is in progress.
  bool restoring_session_ = false;

  // Observer for the active web state in |browser_|'s web state list.
  std::unique_ptr<AllWebStateObservationForwarder> all_web_state_observer_;
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_SESSION_RESTORATION_BROWSER_AGENT_H_
