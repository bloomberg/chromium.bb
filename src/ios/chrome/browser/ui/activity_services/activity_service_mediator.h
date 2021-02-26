// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITY_SERVICE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITY_SERVICE_MEDIATOR_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/activity_services/activity_scenario.h"

namespace bookmarks {
class BookmarkModel;
}

@protocol BookmarksCommands;
@protocol BrowserCommands;
@class ChromeActivityImageSource;
@protocol ChromeActivityItemSource;
@class ChromeActivityURLSource;
@protocol FindInPageCommands;
class PrefService;
@protocol QRGenerationCommands;
@class ShareImageData;
@class ShareToData;

// Mediator used to generate activities.
@interface ActivityServiceMediator : NSObject

// Initializes a mediator instance with a |handler| used to execute action, a
// |bookmarksHandler| to execute Bookmarks actions, a
// |qrGenerationHandler| to execute QR generation actions, a |prefService| to
// read settings and policies, and a |bookmarkModel| to retrieve bookmark
// states.
- (instancetype)initWithHandler:(id<BrowserCommands, FindInPageCommands>)handler
               bookmarksHandler:(id<BookmarksCommands>)bookmarksHandler
            qrGenerationHandler:(id<QRGenerationCommands>)qrGenerationHandler
                    prefService:(PrefService*)prefService
                  bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Generates an array of activity items to be shared via an activity view for
// the given |data|.
- (NSArray<id<ChromeActivityItemSource>>*)activityItemsForData:
    (ShareToData*)data;

// Generates an array of activities to be added to the activity view for the
// given |data|.
- (NSArray*)applicationActivitiesForData:(ShareToData*)data;

// Generates an array of activity items to be shared via an activity view for
// the given |data|.
- (NSArray<ChromeActivityImageSource*>*)activityItemsForImageData:
    (ShareImageData*)data;

// Generates an array of activities to be added to the activity view for the
// given |data|.
- (NSArray*)applicationActivitiesForImageData:(ShareImageData*)data;

// Returns the union of excluded activity types given |items| to share.
- (NSSet*)excludedActivityTypesForItems:
    (NSArray<id<ChromeActivityItemSource>>*)items;

// Handles metric reporting when a sharing |scenario| is initiated.
- (void)shareStartedWithScenario:(ActivityScenario)scenario;

// Handles completion of a share |scenario| with a given action's
// |activityType|. The value of |completed| represents whether the activity
// was completed successfully or not.
- (void)shareFinishedWithScenario:(ActivityScenario)scenario
                     activityType:(NSString*)activityType
                        completed:(BOOL)completed;

@end

#endif  // IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITY_SERVICE_MEDIATOR_H_
