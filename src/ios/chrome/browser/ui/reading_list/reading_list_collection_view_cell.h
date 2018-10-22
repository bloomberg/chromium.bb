// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_COLLECTION_VIEW_CELL_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_COLLECTION_VIEW_CELL_H_

#import "ios/chrome/browser/ui/reading_list/reading_list_ui_distillation_status.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

@class FaviconViewNew;

// Cell for ReadingListCollectionViewItem.
@interface ReadingListCell : MDCCollectionViewCell

// Title label.
@property(nonatomic, readonly, strong) UILabel* titleLabel;
// Subtitle label.
@property(nonatomic, readonly, strong) UILabel* subtitleLabel;
// Timestamp of the distillation in microseconds since Jan 1st 1970.
@property(nonatomic, readonly, strong) UILabel* distillationDateLabel;
// Size of the distilled files.
@property(nonatomic, readonly, strong) UILabel* distillationSizeLabel;
// Whether to show |distillationDateLabel| and |distillationSizeLabel|.
@property(nonatomic, assign) BOOL showDistillationInfo;
// View for displaying the favicon for the reading list entry.
@property(nonatomic, readonly, strong) FaviconViewNew* faviconView;
// Status of the offline version. Updates the visual indicator when updated.
@property(nonatomic, assign) ReadingListUIDistillationStatus distillationState;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_COLLECTION_VIEW_CELL_H_
