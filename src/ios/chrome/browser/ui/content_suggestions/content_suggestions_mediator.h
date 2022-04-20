// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_MEDIATOR_H_

#import <UIKit/UIKit.h>

#include <memory>

#include "components/prefs/pref_service.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_gesture_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_consumer.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_consumer.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_recent_tab_removal_observer_bridge.h"

namespace favicon {
class LargeIconService;
}

namespace ntp_tiles {
class MostVisitedSites;
}

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

@protocol ApplicationCommands;
class Browser;
@protocol BrowserCommands;
@protocol ContentSuggestionsCollectionConsumer;
@protocol DiscoverFeedDelegate;
class GURL;
class LargeIconCache;
class NotificationPromoWhatsNew;
class ReadingListModel;
@protocol SnackbarCommands;
class WebStateList;

// Mediator for ContentSuggestions.
@interface ContentSuggestionsMediator
    : NSObject <ContentSuggestionsCommands,
                ContentSuggestionsGestureCommands,
                StartSurfaceRecentTabObserving>

// Default initializer.
- (instancetype)
         initWithLargeIconService:(favicon::LargeIconService*)largeIconService
                   largeIconCache:(LargeIconCache*)largeIconCache
                  mostVisitedSite:(std::unique_ptr<ntp_tiles::MostVisitedSites>)
                                      mostVisitedSites
                 readingListModel:(ReadingListModel*)readingListModel
                      prefService:(PrefService*)prefService
    isGoogleDefaultSearchProvider:(BOOL)isGoogleDefaultSearchProvider
                          browser:(Browser*)browser NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Registers the feature preferences.
+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry;

// Dispatcher.
@property(nonatomic, weak)
    id<ApplicationCommands, BrowserCommands, SnackbarCommands>
        dispatcher;

// Command handler for the mediator.
@property(nonatomic, weak)
    id<ContentSuggestionsCommands, ContentSuggestionsGestureCommands>
        commandHandler;

// Delegate used to communicate to communicate events to the DiscoverFeed.
@property(nonatomic, weak) id<DiscoverFeedDelegate> discoverFeedDelegate;

// The consumer that will be notified when the data change.
@property(nonatomic, weak) id<ContentSuggestionsCollectionConsumer>
    collectionConsumer;
@property(nonatomic, weak) id<ContentSuggestionsConsumer> consumer;

// YES if the Start Surface is being shown.
@property(nonatomic, assign) BOOL showingStartSurface;

// WebStateList associated with this mediator.
@property(nonatomic, assign) WebStateList* webStateList;

// The web state associated with this NTP.
@property(nonatomic, assign) web::WebState* webState;

// Disconnects the mediator.
- (void)disconnect;

// Reloads content suggestions with most updated model state.
- (void)reloadAllData;

// Trigger a refresh of the Content Suggestions Most Visited tiles.
- (void)refreshMostVisitedTiles;

// The notification promo owned by this mediator.
- (NotificationPromoWhatsNew*)notificationPromo;

// Block |URL| from Most Visited sites.
- (void)blockMostVisitedURL:(GURL)URL;

// Always allow |URL| in Most Visited sites.
- (void)allowMostVisitedURL:(GURL)URL;

// Get the maximum number of sites shown.
+ (NSUInteger)maxSitesShown;

// Whether the most recent tab tile is being shown. Returns YES if
// configureMostRecentTabItemWithWebState: has been called.
- (BOOL)mostRecentTabStartSurfaceTileIsShowing;

// Configures the most recent tab item with |webState| and |timeLabel|.
- (void)configureMostRecentTabItemWithWebState:(web::WebState*)webState
                                     timeLabel:(NSString*)timeLabel;

// Indicates that the "Return to Recent Tab" tile should be hidden.
- (void)hideRecentTabTile;

// Indicates that the NTP promo should be hidden.
- (void)hidePromo;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_MEDIATOR_H_
