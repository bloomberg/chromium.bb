// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_WEB_NAVIGATION_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_WEB_WEB_NAVIGATION_BROWSER_AGENT_H_

#import "ios/chrome/browser/main/browser_user_data.h"

class Browser;
@protocol WebNavigationNTPDelegate;
class WebStateList;

// A browser agent that encapsulates logic for common web navigation tasks on
// the current active web state in the associated browser.
class WebNavigationBrowserAgent
    : public BrowserUserData<WebNavigationBrowserAgent> {
 public:
  // Not copyable or moveable.
  WebNavigationBrowserAgent(const WebNavigationBrowserAgent&) = delete;
  WebNavigationBrowserAgent& operator=(const WebNavigationBrowserAgent&) =
      delete;
  ~WebNavigationBrowserAgent() override;

  // Sets an optional delegate if NTP navigation needs to be handled. If this
  // is not set, no special considerations for the NTP are made.
  void SetDelegate(id<WebNavigationNTPDelegate> delegate);

  // All of the following methods will silently no-op (or return false) if there
  // is no active web state in the assoicated browser's WebStateList.

  // True if it is possible to navigate back.
  bool CanGoBack();
  // Navigates back.
  void GoBack();
  // True if it is possible to navigate forward.
  bool CanGoForward();
  // Navigates forward.
  void GoForward();
  // Stops the active web state's loading.
  void StopLoading();
  // Reloads the active web state.
  void Reload();

 private:
  friend class BrowserUserData<WebNavigationBrowserAgent>;
  explicit WebNavigationBrowserAgent(Browser* browser);
  BROWSER_USER_DATA_KEY_DECL();

  // The web state list for the associated browser. This should never be
  // null.
  WebStateList* web_state_list_;
  // The delegate, if assigned. This may be nil.
  id<WebNavigationNTPDelegate> delegate_;
};

#endif  // IOS_CHROME_BROWSER_WEB_WEB_NAVIGATION_BROWSER_AGENT_H_
