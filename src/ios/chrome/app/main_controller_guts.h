// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_MAIN_CONTROLLER_GUTS_H_
#define IOS_CHROME_APP_MAIN_CONTROLLER_GUTS_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#include "ios/chrome/app/startup/chrome_app_startup_parameters.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remove_mask.h"
#import "ios/chrome/browser/crash_report/crash_restore_helper.h"
#import "ios/chrome/browser/ui/commands/browsing_data_commands.h"

@class AppState;
class ChromeBrowserState;

// TODO(crbug.com/1012697): Remove this protocol when SceneController is
// operational. Move the private internals back into MainController, and pass
// ownership of Scene-related objects to SceneController.
@protocol MainControllerGuts <StartupInformation, BrowsingDataCommands>

// Parameters received at startup time when the app is launched from another
// app.
@property(nonatomic, strong) AppStartupParameters* startupParameters;

// Keeps track of the restore state during startup.
@property(nonatomic, strong) CrashRestoreHelper* restoreHelper;

- (ChromeBrowserState*)mainBrowserState;
- (UIWindow*)window;
- (NSDictionary*)launchOptions;
- (AppState*)appState;

- (void)removeBrowsingDataForBrowserState:(ChromeBrowserState*)browserState
                               timePeriod:(browsing_data::TimePeriod)timePeriod
                               removeMask:(BrowsingDataRemoveMask)removeMask
                          completionBlock:(ProceduralBlock)completionBlock;
// MainController tracks EULA acceptance and performs delayed tasks when the
// first run UI is dismissed.
- (void)prepareForFirstRunUI;
// Returns whether or not the app can launch in incognito mode.
- (BOOL)canLaunchInIncognito;

@end

#endif  // IOS_CHROME_APP_MAIN_CONTROLLER_GUTS_H_
