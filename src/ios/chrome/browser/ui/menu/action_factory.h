// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MENU_ACTION_FACTORY_H_
#define IOS_CHROME_BROWSER_UI_MENU_ACTION_FACTORY_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/ui/menu/menu_action_type.h"
#import "ios/chrome/browser/ui/menu/menu_histograms.h"

class GURL;

// Factory providing methods to create UIActions with consistent titles, images
// and metrics structure.
@interface ActionFactory : NSObject

// Initializes a factory instance to create action instances for the given
// |scenario|.
- (instancetype)initWithScenario:(MenuScenario)scenario;

// Creates a UIAction instance configured with the given |title| and |image|.
// Upon execution, the action's |type| will be recorded and the |block| will be
// run.
- (UIAction*)actionWithTitle:(NSString*)title
                       image:(UIImage*)image
                        type:(MenuActionType)type
                       block:(ProceduralBlock)block;

// Creates a UIAction instance configured to copy the given |URL| to the
// pasteboard.
- (UIAction*)actionToCopyURL:(const GURL)URL;

// Creates a UIAction instance configured for sharing which will invoke
// the given |block| upon execution.
- (UIAction*)actionToShareWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance configured for deletion which will invoke
// the given delete |block| when executed.
- (UIAction*)actionToDeleteWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance whose title and icon are configured for opening a
// URL in a new tab. When triggered, the action will invoke the |block| which
// needs to open a URL in a new tab.
- (UIAction*)actionToOpenInNewTabWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance whose title and icon are configured for opening
// multiple URLs in new tabs. When triggered, the action will invoke the |block|
// which needs to open URLs in new tabs.
- (UIAction*)actionToOpenAllTabsWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance configured for suppression which will invoke
// the given delete |block| when executed.
- (UIAction*)actionToRemoveWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance whose title and icon are configured for editing
// which will invoke the given edit |block| when executed.
- (UIAction*)actionToEditWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance configured for hiding which will invoke
// the given hiding |block| when executed.
- (UIAction*)actionToHideWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance configured for moving a folder which will invoke
// the given |block| when executed.
- (UIAction*)actionToMoveFolderWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance configured for marking an entry from the
// ReadingList as read, which will invoke the given |block| when executed.
- (UIAction*)actionToMarkAsReadWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance configured for marking an entry from the
// ReadingList as unread, which will invoke the given |block| when executed.
- (UIAction*)actionToMarkAsUnreadWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance whose title and icon are configured for viewing
// an offline version of an URL in a new tab. When triggered, the action will
// invoke the |block| when executed.
- (UIAction*)actionToOpenOfflineVersionInNewTabWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance for adding to the reading list.
- (UIAction*)actionToAddToReadingListWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance for adding to bookmarks.
- (UIAction*)actionToBookmarkWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance for editing a bookmark.
- (UIAction*)actionToEditBookmarkWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance for closing a tab.
- (UIAction*)actionToCloseTabWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance for saving an image.
- (UIAction*)actionSaveImageWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance for copying an image.
- (UIAction*)actionCopyImageWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance for searching an image with given search service
// |title|. Invokes the given |completion| block after execution.
- (UIAction*)actionSearchImageWithTitle:(NSString*)title
                                  Block:(ProceduralBlock)block;
// Creates a UIAction instance for closing all tabs.
- (UIAction*)actionToCloseAllTabsWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance for entering tab selection mode.
- (UIAction*)actionToSelectTabsWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance for searching an image with Lens.
// Invokes the given |completion| block after execution.
- (UIAction*)actionToSearchImageUsingLensWithBlock:(ProceduralBlock)block;

@end

#endif  // IOS_CHROME_BROWSER_UI_MENU_ACTION_FACTORY_H_
