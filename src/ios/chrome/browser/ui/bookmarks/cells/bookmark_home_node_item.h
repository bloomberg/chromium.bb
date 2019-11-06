// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_CELLS_BOOKMARK_HOME_NODE_ITEM_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_CELLS_BOOKMARK_HOME_NODE_ITEM_H_

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

namespace bookmarks {
class BookmarkNode;
}

// BookmarkHomeNodeItem provides data for a table view row that displays a
// single bookmark.
@interface BookmarkHomeNodeItem : TableViewItem

// The BookmarkNode that backs this item.
@property(nonatomic, readwrite, assign)
    const bookmarks::BookmarkNode* bookmarkNode;

- (instancetype)initWithType:(NSInteger)type
                bookmarkNode:(const bookmarks::BookmarkNode*)node
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithType:(NSInteger)type NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_CELLS_BOOKMARK_HOME_NODE_ITEM_H_
