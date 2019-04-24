// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_SERVICE_H_
#define IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_SERVICE_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include "components/keyed_service/core/keyed_service.h"
#import "ios/chrome/browser/ui/chrome_load_params.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/web/public/navigation_manager.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

class Browser;
class UrlLoadingNotifier;

@class OpenNewTabCommand;

// TODO(crbug.com/907527): normalize all parameters to open a url in
// UrlLoadingService and URLLoadingServiceDelegate.

// Objective-C delegate for UrlLoadingService.
@protocol URLLoadingServiceDelegate

// Implementing delegate must open the url in |command| in a new tab.
- (void)openURLInNewTabWithCommand:(OpenNewTabCommand*)command;

// Implementing delegate can do an animation using information in |command| when
// opening a background tab, then call |completion|.
- (void)animateOpenBackgroundTabFromCommand:(OpenNewTabCommand*)command
                                 completion:(void (^)())completion;

@end

// Service used to load url in current or new tab.
class UrlLoadingService : public KeyedService {
 public:
  UrlLoadingService(UrlLoadingNotifier* notifier);

  void SetDelegate(id<URLLoadingServiceDelegate> delegate);
  void SetBrowser(Browser* browser);

  id<UrlLoader> GetUrlLoader();

  // Opens a url based on |chrome_params|.
  virtual void LoadUrlInCurrentTab(const ChromeLoadParams& chrome_params);

  // Switches to a tab that matches |web_params| or opens in a new tab.
  virtual void SwitchToTab(
      const web::NavigationManager::WebLoadParams& web_params);

  // Opens a url based on |command| in a new tab.
  virtual void OpenUrlInNewTab(OpenNewTabCommand* command);

 private:
  __weak id<URLLoadingServiceDelegate> delegate_;
  Browser* browser_;
  UrlLoadingNotifier* notifier_;
  id<UrlLoader> url_loader_;
};

#endif  // IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_SERVICE_H_
