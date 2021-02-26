// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_URL_OPENER_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_URL_OPENER_H_

#import <UIKit/UIKit.h>

@class AppState;
@protocol ConnectionInformation;
@protocol StartupInformation;
@protocol TabOpening;
@class URLOpenerParams;

// Handles the URL-opening methods of the ApplicationDelegate. This class has
// only class methods and should not be instantiated.
@interface URLOpener : NSObject

- (instancetype)init NS_UNAVAILABLE;

+ (BOOL)openURL:(URLOpenerParams*)options
        applicationActive:(BOOL)applicationActive
                tabOpener:(id<TabOpening>)tabOpener
    connectionInformation:(id<ConnectionInformation>)connectionInformation
       startupInformation:(id<StartupInformation>)startupInformation;

// Handles open URL at application startup.
+ (void)handleLaunchOptions:(URLOpenerParams*)options
                  tabOpener:(id<TabOpening>)tabOpener
      connectionInformation:(id<ConnectionInformation>)connectionInformation
         startupInformation:(id<StartupInformation>)startupInformation
                   appState:(AppState*)appState;
@end

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_URL_OPENER_H_
