// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_CELLS_BOOKMARK_TABLE_CELL_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_CELLS_BOOKMARK_TABLE_CELL_H_

#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_table_cell_title_editing.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

@class BookmarkTableCell;

// Cell to display bookmark folders and URLs.
// |---------------------------------------------|
// |                                             |
// |[Favicon] [title]                         [>]|
// |                                             |
// |---------------------------------------------|
//

@interface BookmarkTableCell : UITableViewCell<BookmarkTableCellTitleEditing>

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Identifier for -[UITableView registerClass:forCellWithReuseIdentifier:].
+ (NSString*)reuseIdentifier;

// Sets the favicon image.
- (void)setImage:(UIImage*)image;

// Sets placeholder text, when favicon is missing.
- (void)setPlaceholderText:(NSString*)text
                 textColor:(UIColor*)textColor
           backgroundColor:(UIColor*)backgroundColor;

// Set the bookmark node this cell shows.
- (void)setNode:(const bookmarks::BookmarkNode*)node;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_CELLS_BOOKMARK_TABLE_CELL_H_
