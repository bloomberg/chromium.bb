// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_TAB_H_
#define IOS_CHROME_BROWSER_TABS_TAB_H_

#import <UIKit/UIKit.h>

#include <memory>
#include <vector>

#include "ios/net/request_tracker.h"
#include "ios/web/public/user_agent.h"
#include "ui/base/page_transition_types.h"

@class AutofillController;
@class CastController;
class GURL;
@class OpenInController;
@class OverscrollActionsController;
@protocol OverscrollActionsControllerDelegate;
@class PasswordController;
@class SnapshotManager;
@class FormSuggestionController;
@protocol TabDialogDelegate;
@class Tab;

namespace web {
class WebState;
}

// Notification sent by a Tab when it starts to load a new URL. This
// notification must only be used for crash reporting as it is also sent for
// pre-rendered tabs.
extern NSString* const kTabUrlStartedLoadingNotificationForCrashReporting;

// Notification sent by a Tab when it is likely about to start loading a new
// URL. This notification must only be used for crash reporting as it is also
// sent for pre-rendered tabs.
extern NSString* const kTabUrlMayStartLoadingNotificationForCrashReporting;

// Notification sent by a Tab when it is showing an exportable file (e.g a pdf
// file.
extern NSString* const kTabIsShowingExportableNotificationForCrashReporting;

// Notification sent by a Tab when it is closing its current document, to go to
// another location.
extern NSString* const kTabClosingCurrentDocumentNotificationForCrashReporting;

// The key containing the URL in the userInfo for the
// kTabUrlStartedLoadingForCrashReporting and
// kTabUrlMayStartLoadingNotificationForCrashReporting notifications.
extern NSString* const kTabUrlKey;

// The header name and value for the data reduction proxy to request an image to
// be reloaded without optimizations.
extern NSString* const kProxyPassthroughHeaderName;
extern NSString* const kProxyPassthroughHeaderValue;

// Information related to a single tab. The WebState is similar to desktop
// Chrome's WebContents in that it encapsulates rendering. Acts as the
// delegate for the WebState in order to process info about pages having
// loaded.
@interface Tab : NSObject

// The Webstate associated with this Tab.
@property(nonatomic, readonly) web::WebState* webState;

@property(nonatomic, readonly)
    OverscrollActionsController* overscrollActionsController;
@property(nonatomic, weak) id<OverscrollActionsControllerDelegate>
    overscrollActionsControllerDelegate;

// Delegate used to show HTTP Authentication dialogs.
@property(nonatomic, weak) id<TabDialogDelegate> dialogDelegate;

// Creates a new Tab with the given WebState.
- (instancetype)initWithWebState:(web::WebState*)webState;

- (instancetype)init NS_UNAVAILABLE;

// The view that generates print data when printing. It can be nil when printing
// is not supported with this tab. It can be different from |Tab view|.
- (UIView*)viewForPrinting;

// Halts the tab's network activity without closing it. This should only be
// called during shutdown, since the tab will be unusable but still present
// after this method completes.
- (void)terminateNetworkActivity;

// Dismisses all modals owned by the tab.
- (void)dismissModals;

// Called before capturing a snapshot for Tab.
- (void)willUpdateSnapshot;

// Sends a notification to indicate that |url| is going to start loading.
- (void)notifyTabOfUrlMayStartLoading:(const GURL&)url;

@end

#endif  // IOS_CHROME_BROWSER_TABS_TAB_H_
