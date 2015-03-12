// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/ios/cru_context_menu_controller.h"

#include <algorithm>

#include "base/ios/ios_util.h"
#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "ui/base/device_form_factor.h"
#import "ui/base/ios/cru_context_menu_holder.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font_list.h"
#import "ui/gfx/ios/NSString+CrStringDrawing.h"
#include "ui/gfx/text_elider.h"
#include "ui/strings/grit/ui_strings.h"

@implementation CRUContextMenuController {
  // Holds all the titles and actions for the menu.
  base::scoped_nsobject<CRUContextMenuHolder> menuHolder_;
  // The action sheet controller used to display the UI.
  base::scoped_nsobject<UIActionSheet> sheet_;
  // Whether the context menu is visible.
  BOOL visible_;
}


// Clean up and reset for the next time.
- (void)cleanup {
  // iOS 8 fires multiple callbacks when a button is clicked; one round for the
  // button that's clicked and a second round with |buttonIndex| equivalent to
  // tapping outside the context menu. Here the sheet's delegate is reset so
  // that only the first round of callbacks is processed.
  // Note that iOS 8 needs the |sheet_| to stay alive so it's not reset until
  // this CRUContextMenuController is dealloc'd.
  [sheet_ setDelegate:nil];
  menuHolder_.reset();
  visible_ = NO;
}

- (void)dealloc {
  if (visible_) {
    // Context menu must be dismissed explicitly if it is still visible at this
    // stage.
    NSUInteger cancelButtonIndex = [menuHolder_ itemCount];
    [sheet_ dismissWithClickedButtonIndex:cancelButtonIndex animated:NO];
  }
  sheet_.reset();
  [super dealloc];
}

- (BOOL)isVisible {
  return visible_;
}

// Called when the action sheet is dismissed in the modal context menu sheet.
// There is no way to dismiss the sheet without going through this method. Note
// that on iPad this method is called with the index of an nonexistent cancel
// button when the user taps outside the sheet.
- (void)actionSheet:(UIActionSheet*)actionSheet
    didDismissWithButtonIndex:(NSInteger)buttonIndex {
  // On iOS 8, if the user taps an item in the context menu, then taps outside
  // the context menu, the |buttonIndex| passed into this method may be
  // different from the |buttonIndex| passed into
  // |actionsheet:willDismissWithButtonIndex:|. See crbug.com/411894.
  NSUInteger buttonIndexU = buttonIndex;
  // Assumes "cancel" button is last in order.
  if (buttonIndexU < [menuHolder_ itemCount])
    [menuHolder_ performActionAtIndex:buttonIndexU];
  [self cleanup];
}

// Called when the user chooses a button in the modal context menu sheet. Note
// that on iPad this method is called with the index of an nonexistent cancel
// button when the user taps outside the sheet.
- (void)actionSheet:(UIActionSheet*)actionSheet
    clickedButtonAtIndex:(NSInteger)buttonIndex {
  // Some use cases (e.g. opening a new tab on handset) should not wait for the
  // action sheet to animate away before executing the action.
  if ([menuHolder_ shouldDismissImmediatelyOnClickedAtIndex:buttonIndex]) {
    [sheet_ dismissWithClickedButtonIndex:buttonIndex animated:NO];
  }
}

#pragma mark -
#pragma mark WebContextMenuDelegate methods

// Displays a menu using a sheet with the given title.
- (void)showWithHolder:(CRUContextMenuHolder*)menuHolder
               atPoint:(CGPoint)localPoint
                inView:(UIView*)view {
  DCHECK([menuHolder itemCount]);
  menuHolder_.reset([menuHolder retain]);
  // Check that the view is still visible on screen, otherwise just return and
  // don't show the context menu.
  DCHECK([view window] || [view isKindOfClass:[UIWindow class]]);
  if (![view window] && ![view isKindOfClass:[UIWindow class]])
    return;
  CGSize spaceAvailableForTitle =
      [CRUContextMenuController
          availableSpaceForTitleInActionSheetWithMenu:menuHolder_
                                              atPoint:localPoint
                                               inView:view];
  NSString* title = menuHolder.menuTitle;
  if (title) {
    // Show at least one line of text, even if that means the UIActionSheet's
    // items will need to scroll.
    const CGFloat kMinimumVerticalSpace = 21;
    spaceAvailableForTitle.height =
        std::max(kMinimumVerticalSpace, spaceAvailableForTitle.height);
    title = [title cr_stringByElidingToFitSize:spaceAvailableForTitle];
  }
  // Create the sheet.
  sheet_.reset(
      [[UIActionSheet alloc] initWithTitle:title
                                  delegate:self
                         cancelButtonTitle:nil
                    destructiveButtonTitle:nil
                         otherButtonTitles:nil]);
  // Add the labels, in order, to the sheet.
  for (NSString* label in [menuHolder_ itemTitles]) {
    [sheet_ addButtonWithTitle:label];
  }
  // Cancel button goes last, to match other browsers.
  [sheet_ addButtonWithTitle:l10n_util::GetNSString(IDS_APP_CANCEL)];
  [sheet_ setCancelButtonIndex:[menuHolder_ itemCount]];
  [sheet_ showFromRect:CGRectMake(localPoint.x, localPoint.y, 1.0, 1.0)
                inView:view
              animated:YES];

  visible_ = YES;
}

// Returns an approximation of the free space available for the title of an
// actionSheet filled with |menu| shown in |view| at |point|.
+ (CGSize)
    availableSpaceForTitleInActionSheetWithMenu:(CRUContextMenuHolder*)menu
                                        atPoint:(CGPoint)point
                                         inView:(UIView*)view {
  BOOL isIpad = ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET;
  if (base::ios::IsRunningOnIOS8OrLater()) {
    // On iOS8 presenting and dismissing a dummy UIActionSheet does not work
    // (http://crbug.com/392245 and rdar://17677745).
    // As a workaround we return an estimation of the space available depending
    // on the device's type.
    const CGFloat kAvailableWidth = 320;
    const CGFloat kAvailableHeightTablet = 200;
    const CGFloat kAvailableHeightPhone = 100;
    if (isIpad) {
      return CGSizeMake(kAvailableWidth, kAvailableHeightTablet);
    }
    return CGSizeMake(kAvailableWidth, kAvailableHeightPhone);
  } else {
    // Creates a dummy UIActionSheet.
    base::scoped_nsobject<UIActionSheet> dummyActionSheet(
        [[UIActionSheet alloc] initWithTitle:nil
                                    delegate:nil
                           cancelButtonTitle:nil
                      destructiveButtonTitle:nil
                           otherButtonTitles:nil]);
    for (NSString* label in [menu itemTitles]) {
      [dummyActionSheet addButtonWithTitle:label];
    }
    [dummyActionSheet addButtonWithTitle:
        l10n_util::GetNSString(IDS_APP_CANCEL)];
    // Temporarily adds the dummy UIActionSheet to |view|.
    [dummyActionSheet showFromRect:CGRectMake(point.x, point.y, 1.0, 1.0)
                            inView:view
                          animated:NO];
    // On iPad the actionsheet is positioned under or over |point| (as opposed
    // to next to it) when the user clicks within approximately 200 points of
    // respectively the top or bottom edge. This reduces the amount of vertical
    // space available for the title, hence the large padding on ipad.
    const int kPaddingiPad = 200;
    const int kPaddingiPhone = 20;
    CGFloat padding = isIpad ? kPaddingiPad : kPaddingiPhone;
    // A title uses the full width of the actionsheet and all the vertical
    // space on the screen.
    CGSize availableSpaceForTitle =
        CGSizeMake([dummyActionSheet frame].size.width,
            [CRUContextMenuController screenHeight] -
            [dummyActionSheet frame].size.height -
            padding);
    [dummyActionSheet dismissWithClickedButtonIndex:0 animated:NO];
    return availableSpaceForTitle;
  }
}

// Returns the screen's height in pixels.
+ (int)screenHeight {
  DCHECK(!base::ios::IsRunningOnIOS8OrLater());
  switch ([[UIApplication sharedApplication] statusBarOrientation]) {
    case UIInterfaceOrientationLandscapeLeft:
    case UIInterfaceOrientationLandscapeRight:
      return [[UIScreen mainScreen] applicationFrame].size.width;
    case UIInterfaceOrientationPortraitUpsideDown:
    case UIInterfaceOrientationPortrait:
    case UIInterfaceOrientationUnknown:
      return [[UIScreen mainScreen] applicationFrame].size.height;
  }
}

@end

@implementation CRUContextMenuController (UsedForTesting)
- (UIActionSheet*)sheet {
  return sheet_.get();
}
@end
