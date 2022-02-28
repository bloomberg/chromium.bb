// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_TAB_PARENTING_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_TABS_TAB_PARENTING_BROWSER_AGENT_H_

#include "ios/chrome/browser/main/browser_observer.h"
#include "ios/chrome/browser/main/browser_user_data.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer.h"

class TabParentingBrowserAgent
    : BrowserObserver,
      public BrowserUserData<TabParentingBrowserAgent>,
      WebStateListObserver {
 public:
  ~TabParentingBrowserAgent() override;
  TabParentingBrowserAgent(const TabParentingBrowserAgent&) = delete;
  TabParentingBrowserAgent& operator=(const TabParentingBrowserAgent&) = delete;

  // BrowserObserver implementation.
  void BrowserDestroyed(Browser* browser) override;

  // WebStateListObserver implementation.
  void WebStateInsertedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index,
                          bool activating) override;
  void WebStateReplacedAt(WebStateList* web_state_list,
                          web::WebState* old_web_state,
                          web::WebState* new_web_state,
                          int index) override;

 private:
  explicit TabParentingBrowserAgent(Browser* browser);

  friend class BrowserUserData<TabParentingBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_TABS_TAB_PARENTING_BROWSER_AGENT_H_
