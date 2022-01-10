// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BROWSER_AGENT_H_

#include <CoreFoundation/CoreFoundation.h>

#include <string>
#include <vector>

#include "components/send_tab_to_self/send_tab_to_self_model_observer.h"
#import "ios/chrome/browser/main/browser_observer.h"
#import "ios/chrome/browser/main/browser_user_data.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer.h"
#include "ios/web/public/web_state_observer.h"

namespace web {
class WebState;
}

namespace send_tab_to_self {
class SendTabToSelfEntry;
class SendTabToSelfModel;
}  // namespace send_tab_to_self

// Service that listens for SendTabToSelf model changes and calls UI
// handlers to update the UI accordingly.
class SendTabToSelfBrowserAgent
    : public BrowserUserData<SendTabToSelfBrowserAgent>,
      public send_tab_to_self::SendTabToSelfModelObserver,
      public WebStateListObserver,
      public web::WebStateObserver,
      BrowserObserver {
 public:
  ~SendTabToSelfBrowserAgent() override;

  // Add a new entry to the SendTabToSelfModel for the active web state of the
  // browser.
  void SendCurrentTabToDevice(NSString* target_device_id);

  // SendTabToSelfModelObserver::
  // Keeps track of when the model is loaded so that updates to the
  // model can be pushed afterwards.
  void SendTabToSelfModelLoaded() override;
  // Updates the UI to reflect the new entries. Calls the handlers
  // registered through ReceivingUIRegistry.
  void EntriesAddedRemotely(
      const std::vector<const send_tab_to_self::SendTabToSelfEntry*>&
          new_entries) override;
  // Updates the UI to reflect the removal of entries. Calls the handlers
  // registered through ReceivingUIRegistry.
  void EntriesRemovedRemotely(const std::vector<std::string>& guids) override;

  // WebStateListObserver::
  void WebStateActivatedAt(WebStateList* web_state_list,
                           web::WebState* old_web_state,
                           web::WebState* new_web_state,
                           int active_index,
                           ActiveWebStateChangeReason reason) override;

  // WebStateObserver::
  void WasShown(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  explicit SendTabToSelfBrowserAgent(Browser* browser);
  friend class BrowserUserData<SendTabToSelfBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  // BrowserObserver::
  void BrowserDestroyed(Browser* browser) override;

  // Display an infobar for |entry| on the specified |web_state|.
  void DisplayInfoBar(web::WebState* web_state,
                      const send_tab_to_self::SendTabToSelfEntry* entry);

  // Stop observing the WebState and WebStateList and reset associated
  // variables.
  void CleanUpObserversAndVariables();

  // The owning Browser
  Browser* browser_;

  // Owned by the SendTabToSelfSyncService which should outlive this class
  send_tab_to_self::SendTabToSelfModel* model_;

  // The pending SendTabToSelf entry to display an InfoBar for.
  const send_tab_to_self::SendTabToSelfEntry* pending_entry_ = nullptr;

  // The WebState that is being observed for activation, if any.
  web::WebState* pending_web_state_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BROWSER_AGENT_H_
