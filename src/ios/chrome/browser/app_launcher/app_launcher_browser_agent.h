// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_LAUNCHER_APP_LAUNCHER_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_APP_LAUNCHER_APP_LAUNCHER_BROWSER_AGENT_H_

#include "base/scoped_observer.h"
#import "ios/chrome/browser/app_launcher/app_launcher_tab_helper.h"
#import "ios/chrome/browser/app_launcher/app_launcher_tab_helper_delegate.h"
#import "ios/chrome/browser/main/browser_user_data.h"
#import "ios/chrome/browser/tabs/tab_helper_delegate_installer.h"

class OverlayRequestQueue;

// A browser agent that manages opening external apps for navigations that occur
// within one of the Browser's WebStates.
class AppLauncherBrowserAgent
    : public BrowserUserData<AppLauncherBrowserAgent> {
 public:
  ~AppLauncherBrowserAgent() override;

 private:
  explicit AppLauncherBrowserAgent(Browser* browser);
  friend class BrowserUserData<AppLauncherBrowserAgent>;

  // Helper object that handles delegated AppLauncherTabHelper functionality.
  class TabHelperDelegate : public AppLauncherTabHelperDelegate {
   public:
    explicit TabHelperDelegate(WebStateList* web_state_list);
    ~TabHelperDelegate() override;

   private:
    // AppLauncherTabHelperDelegate:
    void LaunchAppForTabHelper(AppLauncherTabHelper* tab_helper,
                               const GURL& url,
                               bool link_transition) override;
    void ShowRepeatedAppLaunchAlert(
        AppLauncherTabHelper* tab_helper,
        base::OnceCallback<void(bool)> completion) override;

    // Returns the OverlayRequestQueue to use for app launch dialogs from
    // |web_state|.  Returns the queue for |web_state|'s opener if |web_state|
    // is expected to be closed before the app launcher dialog can be shown.
    OverlayRequestQueue* GetQueueForAppLaunchDialog(web::WebState* web_state);

    // The Browser's WebStateList.  Used to fetch the appropriate request queue
    // for app launcher dialogs.
    WebStateList* web_state_list_ = nullptr;
  };

  // Handler for app launches in the Browser.
  TabHelperDelegate tab_helper_delegate_;
  // The tab helper delegate installer.
  TabHelperDelegateInstaller<AppLauncherTabHelper, AppLauncherTabHelperDelegate>
      tab_helper_delegate_installer_;
  // BrowserUserData key.
  BROWSER_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_APP_LAUNCHER_APP_LAUNCHER_BROWSER_AGENT_H_
