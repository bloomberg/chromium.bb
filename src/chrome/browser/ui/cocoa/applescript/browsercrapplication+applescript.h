// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BROWSERCRAPPLICATION_APPLESCRIPT_H_
#define CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BROWSERCRAPPLICATION_APPLESCRIPT_H_

#import <Cocoa/Cocoa.h>

#import "chrome/browser/chrome_browser_application_mac.h"

@class BookmarkFolderAppleScript;
@class WindowAppleScript;

// Represent the top level application scripting object in applescript.
@interface BrowserCrApplication (AppleScriptAdditions)

// Application window manipulation methods.
// Returns an array of |WindowAppleScript*| of all windows present in the
// application.
- (NSArray*)appleScriptWindows;

// Inserts a window at the beginning.
- (void)insertInAppleScriptWindows:(WindowAppleScript*)aWindow;

// Inserts a window at some position in the list.
// Called by applescript which takes care of bounds checking, make sure of it
// before calling directly.
- (void)insertInAppleScriptWindows:(WindowAppleScript*)aWindow
                           atIndex:(int)index;

// Removes a window from the list.
// Called by applescript which takes care of bounds checking, make sure of it
// before calling directly.
- (void)removeFromAppleScriptWindowsAtIndex:(int)index;

// Always returns nil to indicate that it is the root container object.
- (NSScriptObjectSpecifier*)objectSpecifier;

// Returns the other bookmarks bookmark folder,
// returns nil if there is an error.
- (BookmarkFolderAppleScript*)otherBookmarks;

// Returns the bookmarks bar bookmark folder, return nil if there is an error.
- (BookmarkFolderAppleScript*)bookmarksBar;

// Returns the Bookmarks Bar and Other Bookmarks Folders, each is of type
// |BookmarkFolderAppleScript*|.
- (NSArray*)bookmarkFolders;

// Required functions, even though bookmarkFolders is declared as
// read-only, cocoa scripting does not currently prevent writing.
- (void)insertInBookmarksFolders:(id)aBookmarkFolder;
- (void)insertInBookmarksFolders:(id)aBookmarkFolder atIndex:(int)index;
- (void)removeFromBookmarksFoldersAtIndex:(int)index;

@end

#endif// CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BROWSERCRAPPLICATION_APPLESCRIPT_H_
