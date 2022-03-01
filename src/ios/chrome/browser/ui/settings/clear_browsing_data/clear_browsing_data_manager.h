// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_CLEAR_BROWSING_DATA_MANAGER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_CLEAR_BROWSING_DATA_MANAGER_H_

#include "components/browsing_data/core/counters/browsing_data_counter.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"

class Browser;
class BrowsingDataRemover;
enum class BrowsingDataRemoveMask;
class ChromeBrowserState;

@class ActionSheetCoordinator;
@class BrowsingDataCounterWrapperProducer;
@protocol ClearBrowsingDataConsumer;
@protocol CollectionViewFooterLinkDelegate;

// Clear Browswing Data Section Identifiers.
enum ClearBrowsingDataSectionIdentifier {
  // Section holding types of data that can be cleared.
  SectionIdentifierDataTypes = kSectionIdentifierEnumZero,
  // Section for informational footnote about user's Google Account data.
  SectionIdentifierGoogleAccount,
  // Section for footnote about synced data being cleared & site settings
  // remaining.
  SectionIdentifierSavedSiteData,
  // Section containing cell displaying time range to remove data.
  SectionIdentifierTimeRange,
};

// Clear Browsing Data Item Types.
enum ClearBrowsingDataItemType {
  // Item representing browsing history data.
  ItemTypeDataTypeBrowsingHistory = kItemTypeEnumZero,
  // Item representing cookies and site data.
  ItemTypeDataTypeCookiesSiteData,
  // Items representing cached data.
  ItemTypeDataTypeCache,
  // Items representing saved passwords.
  ItemTypeDataTypeSavedPasswords,
  // Items representing autofill data.
  ItemTypeDataTypeAutofill,
  // Footer noting account will not be signed out.
  ItemTypeFooterGoogleAccount,
  // Footer noting user will not be signed out of chrome and other forms of
  // browsing history will still be available.
  ItemTypeFooterGoogleAccountAndMyActivity,
  // Footer noting site settings will remain.
  ItemTypeFooterSavedSiteData,
  // Footer noting data will be cleared on all devices except for saved
  // settings.
  ItemTypeFooterClearSyncAndSavedSiteData,
  // Item showing time range to remove data and allowing user to edit time
  // range.
  ItemTypeTimeRange,
  // Footer noting user will not be signed out of chrome and how to delete
  // search history based on set default search engine (DSE).
  ItemTypeFooterGoogleAccountDSEBased,
};

// Manager that serves as the bulk of the logic for
// ClearBrowsingDataConsumer.
@interface ClearBrowsingDataManager : NSObject

// The manager's consumer.
@property(nonatomic, weak) id<ClearBrowsingDataConsumer> consumer;

// Default init method. |browserState| can't be nil.
- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState;

// Designated initializer to allow dependency injection (in tests).
- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState
                   browsingDataRemover:(BrowsingDataRemover*)remover
    browsingDataCounterWrapperProducer:
        (BrowsingDataCounterWrapperProducer*)producer NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Fills |model| with appropriate sections and items.
- (void)loadModel:(ListModel*)model;

// Restarts browsing data counters, which in turn updates UI, with those data
// types specified by |mask|.
- (void)restartCounters:(BrowsingDataRemoveMask)mask;

// Returns a ActionSheetCoordinator that has action block to clear data of type
// |dataTypeMaskToRemove|.
- (ActionSheetCoordinator*)
    actionSheetCoordinatorWithDataTypesToRemove:
        (BrowsingDataRemoveMask)dataTypeMaskToRemove
                             baseViewController:
                                 (UIViewController*)baseViewController
                                        browser:(Browser*)browser
                            sourceBarButtonItem:
                                (UIBarButtonItem*)sourceBarButtonItem;

// Get the text to be displayed by a counter from the given |result|
- (NSString*)counterTextFromResult:
    (const browsing_data::BrowsingDataCounter::Result&)result;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_CLEAR_BROWSING_DATA_MANAGER_H_
