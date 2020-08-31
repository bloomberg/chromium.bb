// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/activities/bookmark_activity.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/policy/policy_features.h"
#include "ios/chrome/browser/ui/commands/browser_commands.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kBookmarkActivityType = @"com.google.chrome.bookmarkActivity";

}  // namespace

@interface BookmarkActivity ()
// Whether or not the page is bookmarked.
@property(nonatomic, assign) BOOL bookmarked;
// The bookmark model used to validate if a page was bookmarked.
@property(nonatomic, assign) bookmarks::BookmarkModel* bookmarkModel;
// The URL for the activity.
@property(nonatomic, assign) GURL URL;
// The handler invoked when the activity is performed.
@property(nonatomic, weak) id<BrowserCommands> handler;
// User's preferences service.
@property(nonatomic, assign) PrefService* prefService;
@end

@implementation BookmarkActivity

- (instancetype)initWithURL:(const GURL&)URL
              bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                    handler:(id<BrowserCommands>)handler
                prefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _URL = URL;
    _bookmarkModel = bookmarkModel;
    _handler = handler;
    _prefService = prefService;

    _bookmarked = _bookmarkModel && _bookmarkModel->loaded() &&
                  _bookmarkModel->IsBookmarked(_URL);
  }
  return self;
}

#pragma mark - UIActivity

- (NSString*)activityType {
  return kBookmarkActivityType;
}

- (NSString*)activityTitle {
  if (self.bookmarked)
    return l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_EDIT_BOOKMARK);
  return l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_ADD_TO_BOOKMARKS);
}

- (UIImage*)activityImage {
  if (self.bookmarked)
    return [UIImage imageNamed:@"activity_services_edit_bookmark"];
  return [UIImage imageNamed:@"activity_services_add_bookmark"];
}

- (BOOL)canPerformWithActivityItems:(NSArray*)activityItems {
  // Don't show the add/remove bookmark activity if we have an invalid
  // bookmarkModel, or if editing bookmarks is disabled in the prefs.
  return self.bookmarkModel && [self isEditBookmarksEnabledInPrefs];
}

- (void)prepareWithActivityItems:(NSArray*)activityItems {
}

+ (UIActivityCategory)activityCategory {
  return UIActivityCategoryAction;
}

- (void)performActivity {
  [self.handler bookmarkPage];
  [self activityDidFinish:YES];
}

#pragma mark - Private

// Verifies if, based on preferences, the user can edit their bookmarks or not.
- (BOOL)isEditBookmarksEnabledInPrefs {
  if (IsEditBookmarksIOSEnabled())
    return self.prefService->GetBoolean(
        bookmarks::prefs::kEditBookmarksEnabled);
  return YES;
}

@end
