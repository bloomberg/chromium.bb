// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_LAUNCHER_APP_LAUNCHER_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_APP_LAUNCHER_APP_LAUNCHER_TAB_HELPER_DELEGATE_H_

#include "ios/chrome/browser/procedural_block_types.h"

class AppLauncherTabHelper;
class GURL;

// Protocol for handling application launching and presenting related UI.
@protocol AppLauncherTabHelperDelegate

// Launches application that has |URL| if possible (optionally after confirming
// via dialog). Returns NO if there is no such application available.
// TODO(crbug.com/850760): Change this method return to void, once the new
// AppLauncherRefresh logic is always enabled.
- (BOOL)appLauncherTabHelper:(AppLauncherTabHelper*)tabHelper
            launchAppWithURL:(const GURL&)URL
              linkTransition:(BOOL)linkTransition;

// Alerts the user that there have been repeated attempts to launch
// the application. |completionHandler| is called with the user's
// response on whether to launch the application.
- (void)appLauncherTabHelper:(AppLauncherTabHelper*)tabHelper
    showAlertOfRepeatedLaunchesWithCompletionHandler:
        (ProceduralBlockWithBool)completionHandler;

@end

#endif  // IOS_CHROME_BROWSER_APP_LAUNCHER_APP_LAUNCHER_TAB_HELPER_DELEGATE_H_
