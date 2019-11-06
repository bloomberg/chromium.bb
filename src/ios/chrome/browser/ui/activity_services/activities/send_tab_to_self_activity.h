// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITIES_SEND_TAB_TO_SELF_ACTIVITY_H_
#define IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITIES_SEND_TAB_TO_SELF_ACTIVITY_H_

#import <UIKit/UIKit.h>

@protocol BrowserCommands;
@protocol ActivityServicePresentation;

// Activity that sends the tab to another of the user's devices.
@interface SendTabToSelfActivity : UIActivity

// Identifier for the send tab to self activity.
+ (NSString*)activityIdentifier;

// Initialize the send tab to self activity with the |dispatcher| that is used
// to add the tab to the other device, |sendTabToSelfTargets| is the list of
// devices that will be presented to the user via |presenter| and |title|
// represents the title of the tab being shared.
- (instancetype)initWithDispatcher:(id<BrowserCommands>)dispatcher
              sendTabToSelfTargets:
                  (NSDictionary<NSString*, NSString*>*)sendTabToSelfTargets
                         presenter:(id<ActivityServicePresentation>)presenter
                             title:(NSString*)title;

@end

#endif  // IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITIES_SEND_TAB_TO_SELF_ACTIVITY_H_