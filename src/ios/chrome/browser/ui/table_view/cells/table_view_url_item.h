// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_URL_ITEM_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_URL_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

class GURL;
@class FaviconView;
@class TableViewURLCellFaviconBadgeView;

// TableViewURLItem contains the model data for a TableViewURLCell.
@interface TableViewURLItem : TableViewItem

// The title of the page at |URL|.
@property(nonatomic, readwrite, copy) NSString* title;
// GURL from which the cell will retrieve a favicon and display the host name.
@property(nonatomic, assign) GURL URL;
// Supplemental text used to describe the URL.
@property(nonatomic, readwrite, copy) NSString* supplementalURLText;
// Delimiter used to separate the URL hostname and the supplemental text.
@property(nonatomic, readwrite, copy) NSString* supplementalURLTextDelimiter;
// Metadata text displayed at the trailing edge of the cell.
@property(nonatomic, readwrite, copy) NSString* metadata;
// The image for the badge view added over the favicon.
@property(nonatomic, readwrite, strong) UIImage* badgeImage;
// Identifier to match a URLItem with its URLCell.
@property(nonatomic, readonly) NSString* uniqueIdentifier;

@end

// TableViewURLCell is used in Bookmarks, Reading List, and Recent Tabs.  It
// contains a favicon, a title, a URL, and optionally some metadata such as a
// timestamp or a file size. After configuring the cell, make sure to call
// configureUILayout:.
@interface TableViewURLCell : TableViewCell

// The imageview that is displayed on the leading edge of the cell.  This
// contains a favicon composited on top of an off-white background.
@property(nonatomic, readonly, strong) FaviconView* faviconView;

// Container View for the faviconView.
@property(nonatomic, readonly, strong) UIImageView* faviconContainerView;

// The image view used to display the favicon badge.
@property(nonatomic, readonly, strong)
    TableViewURLCellFaviconBadgeView* faviconBadgeView;

// The cell title.
@property(nonatomic, readonly, strong) UILabel* titleLabel;

// The host URL associated with this cell.
@property(nonatomic, readonly, strong) UILabel* URLLabel;

// Optional metadata that is displayed at the trailing edge of the cell.
@property(nonatomic, readonly, strong) UILabel* metadataLabel;

// Unique identifier that matches with one URLItem.
@property(nonatomic, strong) NSString* cellUniqueIdentifier;

// Properly configure the subview layouts once all labels' properties have been
// configured. This must be called at the end of configureCell: for all items
// that use TableViewURLCell.
- (void)configureUILayout;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_URL_ITEM_H_
