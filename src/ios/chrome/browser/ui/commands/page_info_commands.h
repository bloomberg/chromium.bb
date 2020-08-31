// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_PAGE_INFO_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_PAGE_INFO_COMMANDS_H_

#include <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

// Commands related to the Page Info UI.
@protocol PageInfoCommands

// TODO(crbug.com/1038923): Remove this command.
// Shows the page security info. |originPoint| is the midpoint of the UI element
// which triggered this command and should be in window coordinates.
- (void)legacyShowPageInfoForOriginPoint:(CGPoint)originPoint;

// Shows the page security info.
- (void)showPageInfo;

// Hides the page security info.
- (void)hidePageInfo;

// Shows the security help page.
- (void)showSecurityHelpPage;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_PAGE_INFO_COMMANDS_H_
