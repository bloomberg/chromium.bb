// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_DATA_SOURCE_H_

#import <UIKit/UIKit.h>

@class CollectionViewItem;
@class ContentSuggestion;
@class ContentSuggestionIdentifier;
@class ContentSuggestionsSectionInformation;
@class FaviconAttributes;
@protocol ContentSuggestionsDataSink;
@protocol ContentSuggestionsImageFetcher;
@protocol SuggestedContent;

namespace content_suggestions {

// Status code for the content suggestions fetches.
typedef NS_ENUM(NSInteger, StatusCode) {
  StatusCodeSuccess,
  StatusCodeError,
  StatusCodePermanentError,
  StatusCodeNotRun,
};

}  // namespace content_suggestions

// Typedef for a block taking the fetched suggestions and the fetch result
// status as parameter.
typedef void (^MoreSuggestionsFetched)(
    NSArray<CollectionViewItem<SuggestedContent>*>* _Nullable,
    content_suggestions::StatusCode status);

// DataSource for the content suggestions. Provides the suggestions data in a
// format compatible with Objective-C.
@protocol ContentSuggestionsDataSource

// The data sink that will be notified when the data change.
@property(nonatomic, nullable, weak) id<ContentSuggestionsDataSink> dataSink;

// Returns all the sections information in the order they should be displayed.
- (nonnull NSArray<ContentSuggestionsSectionInformation*>*)sectionsInfo;

// Returns the items associated with the |sectionInfo|.
- (nonnull NSArray<CollectionViewItem<SuggestedContent>*>*)itemsForSectionInfo:
    (nonnull ContentSuggestionsSectionInformation*)sectionInfo;

// Fetches additional content. All the |knownSuggestions| must come from the
// same |sectionInfo|. If the fetch was completed, the given |callback| is
// called with the new content.
- (void)fetchMoreSuggestionsKnowing:
            (nullable NSArray<ContentSuggestionIdentifier*>*)knownSuggestions
                    fromSectionInfo:
                        (nonnull ContentSuggestionsSectionInformation*)
                            sectionInfo
                           callback:(nonnull MoreSuggestionsFetched)callback;

// Dismisses the suggestion from the content suggestions service. It doesn't
// change the UI.
- (void)dismissSuggestion:
    (nonnull ContentSuggestionIdentifier*)suggestionIdentifier;

// Returns the header view containing the logo and omnibox to be displayed.
- (nullable UIView*)headerViewForWidth:(CGFloat)width;

// Toggles the preference that controls content suggestions articles visilibity
// and triggers an update for the necessary data sources.
- (void)toggleArticlesVisibility;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_DATA_SOURCE_H_
