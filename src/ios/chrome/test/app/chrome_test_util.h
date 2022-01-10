
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_APP_CHROME_TEST_UTIL_H_
#define IOS_CHROME_TEST_APP_CHROME_TEST_UTIL_H_

#include "base/compiler_specific.h"
#import "base/ios/block_types.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"

@protocol ApplicationCommands;
class Browser;
class ChromeBrowserState;
@class MainController;
@class NewTabPageController;
@class SceneController;
@class SceneState;
@class UIViewController;

namespace chrome_test_util {

// Returns the main controller.
MainController* GetMainController();

// Returns the foreground, active scene.
SceneState* GetForegroundActiveScene();

// Returns the foreground, active scene controller.
SceneController* GetForegroundActiveSceneController();

// Returns the number of regular Browsers for the default profile.
NSUInteger RegularBrowserCount();

// Returns the current, non-incognito ChromeBrowserState.
ChromeBrowserState* GetOriginalBrowserState();

// Returns the current incognito ChromeBrowserState
ChromeBrowserState* GetCurrentIncognitoBrowserState();

// Returns the browser for the main interface.
Browser* GetMainBrowser();

// Returns the active view controller.
// NOTE: It is preferred to not directly access the active view controller if
// possible.
UIViewController* GetActiveViewController();

// Returns the dispatcher for the active Browser.
id<ApplicationCommands, BrowserCommands> HandlerForActiveBrowser();

// Removes all presented infobars.
void RemoveAllInfoBars();

// Dismisses all presented views and modal dialogs. |completion| is invoked when
// all the views are dismissed.
void ClearPresentedState(ProceduralBlock completion);

// Sets the value of a boolean local state pref.
// TODO(crbug.com/647022): Clean up other tests that use this helper function.
void SetBooleanLocalStatePref(const char* pref_name, bool value);

// Sets the value of a boolean user pref in the given browser state.
void SetBooleanUserPref(ChromeBrowserState* browser_state,
                        const char* pref_name,
                        bool value);

// Sets the value of an integer user pref in the given browser state.
void SetIntegerUserPref(ChromeBrowserState* browser_state,
                        const char* pref_name,
                        int value);

// Sets the state of first launch.
void SetFirstLaunchStateTo(bool value);

// Checks whether metrics recording is enabled or not.
bool IsMetricsRecordingEnabled();

// Checks whether metrics reporting is enabled or not.
bool IsMetricsReportingEnabled();

// Checks whether breakpad recording is enabled or not.
bool IsBreakpadEnabled();

// Checks whether breakpad reporting is enabled or not.
bool IsBreakpadReportingEnabled();

// Checks whether this is the first launch after upgrade or not.
bool IsFirstLaunchAfterUpgrade();

// Waits for Breakpad to process the queued updates.
void WaitForBreakpadQueue();

// Simulates launching Chrome from another application.
void OpenChromeFromExternalApp(const GURL& url);

// Purges cached web view page, so the next time back navigation will not use
// cached page. Browsers don't have to use fresh version for back forward
// navigation for HTTP pages and may serve version from the cache even if
// Cache-Control response header says otherwise.
bool PurgeCachedWebViewPages() WARN_UNUSED_RESULT;

}  // namespace chrome_test_util

#endif  // IOS_CHROME_TEST_APP_CHROME_TEST_UTIL_H_
