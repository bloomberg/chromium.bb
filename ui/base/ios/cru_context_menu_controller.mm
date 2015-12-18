// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/ios/cru_context_menu_controller.h"

#include <algorithm>

#include "base/ios/ios_util.h"
#include "base/ios/weak_nsobject.h"
#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#include "ui/base/device_form_factor.h"
#import "ui/base/ios/cru_context_menu_holder.h"
#include "ui/base/l10n/l10n_util.h"
#import "ui/gfx/ios/NSString+CrStringDrawing.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

// Returns the screen's height in points.
CGFloat GetScreenHeight() {
  DCHECK(!base::ios::IsRunningOnIOS8OrLater());
  switch ([[UIApplication sharedApplication] statusBarOrientation]) {
    case UIInterfaceOrientationLandscapeLeft:
    case UIInterfaceOrientationLandscapeRight:
      return CGRectGetWidth([[UIScreen mainScreen] bounds]);
    case UIInterfaceOrientationPortraitUpsideDown:
    case UIInterfaceOrientationPortrait:
    case UIInterfaceOrientationUnknown:
      return CGRectGetHeight([[UIScreen mainScreen] bounds]);
  }
}

}  // namespace

// Abstracts system implementation of popovers and action sheets.
@protocol CRUContextMenuControllerImpl<NSObject>

// Whether the context menu is visible.
@property(nonatomic, readonly, getter=isVisible) BOOL visible;

// Displays a context menu.
- (void)showWithHolder:(CRUContextMenuHolder*)menuHolder
               atPoint:(CGPoint)localPoint
                inView:(UIView*)view;
@end

// Backs up CRUContextMenuController on iOS 7 by using UIActionSheet.
@interface CRUActionSheetController
    : NSObject<CRUContextMenuControllerImpl, UIActionSheetDelegate> {
  // The action sheet used to display the UI.
  base::scoped_nsobject<UIActionSheet> _sheet;
  // Holds all the titles and actions for the menu.
  base::scoped_nsobject<CRUContextMenuHolder> _menuHolder;
}
@end

// Backs up CRUContextMenuController on iOS 8 and higher by using
// UIAlertController.
@interface CRUAlertController : NSObject<CRUContextMenuControllerImpl>
// Redefined to readwrite.
@property(nonatomic, readwrite, getter=isVisible) BOOL visible;
@end

// Displays a context menu. Implements Bridge pattern.
@implementation CRUContextMenuController {
  // Implementation specific for iOS version.
  base::scoped_nsprotocol<id<CRUContextMenuControllerImpl>> _impl;
}

- (BOOL)isVisible {
  return [_impl isVisible];
}

- (instancetype)init {
  self = [super init];
  if (self) {
    if (base::ios::IsRunningOnIOS8OrLater()) {
      _impl.reset([[CRUAlertController alloc] init]);
    } else {
      _impl.reset([[CRUActionSheetController alloc] init]);
    }
  }
  return self;
}

- (void)showWithHolder:(CRUContextMenuHolder*)menuHolder
               atPoint:(CGPoint)point
                inView:(UIView*)view {
  DCHECK(menuHolder.itemCount);
  // Check that the view is still visible on screen, otherwise just return and
  // don't show the context menu.
  if (![view window] && ![view isKindOfClass:[UIWindow class]])
    return;
  [_impl showWithHolder:menuHolder atPoint:point inView:view];
}

@end

#pragma mark - iOS 7

@implementation CRUActionSheetController
@synthesize visible = _visible;

- (void)dealloc {
  if (_visible) {
    // Context menu must be dismissed explicitly if it is still visible.
    NSUInteger cancelButtonIndex = [_menuHolder itemCount];
    [_sheet dismissWithClickedButtonIndex:cancelButtonIndex animated:NO];
  }
  [super dealloc];
}

- (void)showWithHolder:(CRUContextMenuHolder*)menuHolder
               atPoint:(CGPoint)point
                inView:(UIView*)view {
  // If the content of UIActionSheet does not fit the screen then scrollbars
  // are added to the menu items area. If that's the case, elide the title to
  // avoid having scrollbars for menu items.
  CGSize spaceAvailableForTitle =
      [self sizeForTitleThatFitsMenuWithHolder:menuHolder
                                       atPoint:point
                                        inView:view];
  NSString* menuTitle = menuHolder.menuTitle;
  if (menuTitle) {
    // Show at least one line of text, even if that means the action sheet's
    // items will need to scroll.
    const CGFloat kMinimumVerticalSpace = 21;
    spaceAvailableForTitle.height =
        std::max(kMinimumVerticalSpace, spaceAvailableForTitle.height);
    menuTitle = [menuTitle cr_stringByElidingToFitSize:spaceAvailableForTitle];
  }

  // Present UIActionSheet.
  _sheet.reset(
      [self newActionSheetWithHolder:menuHolder title:menuTitle delegate:self]);
  [_sheet setCancelButtonIndex:menuHolder.itemCount];
  [_sheet showFromRect:CGRectMake(point.x, point.y, 1.0, 1.0)
                inView:view
              animated:YES];

  _menuHolder.reset([menuHolder retain]);
  _visible = YES;
}

#pragma mark Implementation

// Returns an approximation of the free space available for the title of an
// actionSheet filled with |menu| shown in |view| at |point|.
- (CGSize)sizeForTitleThatFitsMenuWithHolder:(CRUContextMenuHolder*)menuHolder
                                     atPoint:(CGPoint)point
                                      inView:(UIView*)view {
  // Create a dummy UIActionSheet.
  base::scoped_nsobject<UIActionSheet> dummySheet(
      [self newActionSheetWithHolder:menuHolder title:nil delegate:nil]);
  // Temporarily add the dummy UIActionSheet to |view|.
  [dummySheet showFromRect:CGRectMake(point.x, point.y, 1.0, 1.0)
                    inView:view
                  animated:NO];
  // On iPad the actionsheet is positioned under or over |point| (as opposed
  // to next to it) when the user clicks within approximately 200 points of
  // respectively the top or bottom edge. This reduces the amount of vertical
  // space available for the title, hence the large padding on ipad.
  const CGFloat kPaddingiPad = 200;
  const CGFloat kPaddingiPhone = 20;
  BOOL isIPad = ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET;
  const CGFloat padding = isIPad ? kPaddingiPad : kPaddingiPhone;
  // A title uses the full width of the actionsheet and all the vertical
  // space on the screen.
  CGSize result = CGSizeMake(
      CGRectGetWidth([dummySheet frame]),
      GetScreenHeight() - CGRectGetHeight([dummySheet frame]) - padding);
  [dummySheet dismissWithClickedButtonIndex:0 animated:NO];
  return result;
}

// Returns an UIActionSheet. Callers responsible for releasing returned object.
- (UIActionSheet*)newActionSheetWithHolder:(CRUContextMenuHolder*)menuHolder
                                     title:(NSString*)title
                                  delegate:(id<UIActionSheetDelegate>)delegate {
  UIActionSheet* sheet = [[UIActionSheet alloc] initWithTitle:title
                                                     delegate:delegate
                                            cancelButtonTitle:nil
                                       destructiveButtonTitle:nil
                                            otherButtonTitles:nil];

  for (NSString* itemTitle in menuHolder.itemTitles) {
    [sheet addButtonWithTitle:itemTitle];
  }
  [sheet addButtonWithTitle:l10n_util::GetNSString(IDS_APP_CANCEL)];
  return sheet;
}

#pragma mark UIActionSheetDelegate

// Called when the action sheet is dismissed in the modal context menu sheet.
// There is no way to dismiss the sheet without going through this method. Note
// that on iPad this method is called with the index of an nonexistent cancel
// button when the user taps outside the sheet.
- (void)actionSheet:(UIActionSheet*)actionSheet
    didDismissWithButtonIndex:(NSInteger)buttonIndex {
  NSUInteger unsignedButtonIndex = buttonIndex;
  // Assumes "cancel" button is last in order.
  if (unsignedButtonIndex < [_menuHolder itemCount])
    [_menuHolder performActionAtIndex:unsignedButtonIndex];
  _menuHolder.reset();
  _visible = NO;
}

// Called when the user chooses a button in the modal context menu sheet. Note
// that on iPad this method is called with the index of an nonexistent cancel
// button when the user taps outside the sheet.
- (void)actionSheet:(UIActionSheet*)actionSheet
    clickedButtonAtIndex:(NSInteger)buttonIndex {
  // Some use cases (e.g. opening a new tab on handset) should not wait for the
  // action sheet to animate away before executing the action.
  if ([_menuHolder shouldDismissImmediatelyOnClickedAtIndex:buttonIndex]) {
    [_sheet dismissWithClickedButtonIndex:buttonIndex animated:NO];
  }
}

@end

#pragma mark - iOS8 and higher

@implementation CRUAlertController
@synthesize visible = _visible;

- (CGSize)sizeForTitleThatFitsMenuWithHolder:(CRUContextMenuHolder*)menuHolder
                                     atPoint:(CGPoint)point
                                      inView:(UIView*)view {
  // Presenting and dismissing a dummy UIAlertController flushes a screen.
  // As a workaround return an estimation of the space available depending
  // on the device's type.
  const CGFloat kAvailableWidth = 320;
  const CGFloat kAvailableHeightTablet = 200;
  const CGFloat kAvailableHeightPhone = 100;
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return CGSizeMake(kAvailableWidth, kAvailableHeightTablet);
  }
  return CGSizeMake(kAvailableWidth, kAvailableHeightPhone);
}

- (void)showWithHolder:(CRUContextMenuHolder*)menuHolder
               atPoint:(CGPoint)point
                inView:(UIView*)view {
  UIAlertController* alert = [UIAlertController
      alertControllerWithTitle:menuHolder.menuTitle
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];
  alert.popoverPresentationController.sourceView = view;
  alert.popoverPresentationController.sourceRect =
      CGRectMake(point.x, point.y, 1.0, 1.0);

  // Add the actions.
  base::WeakNSObject<CRUAlertController> weakSelf(self);
  [menuHolder.itemTitles enumerateObjectsUsingBlock:^(NSString* itemTitle,
                                                      NSUInteger itemIndex,
                                                      BOOL*) {
    void (^actionHandler)(UIAlertAction*) = ^(UIAlertAction* action) {
      [menuHolder performActionAtIndex:itemIndex];
      [weakSelf setVisible:NO];
    };
    [alert addAction:[UIAlertAction actionWithTitle:itemTitle
                                              style:UIAlertActionStyleDefault
                                            handler:actionHandler]];
  }];

  // Cancel button goes last, to match other browsers.
  void (^cancelHandler)(UIAlertAction*) = ^(UIAlertAction* action) {
    [weakSelf setVisible:NO];
  };
  UIAlertAction* cancel_action =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(IDS_APP_CANCEL)
                               style:UIAlertActionStyleCancel
                             handler:cancelHandler];
  [alert addAction:cancel_action];

  // Present sheet/popover using controller that is added to view hierarchy.
  UIViewController* topController = view.window.rootViewController;
  while (topController.presentedViewController)
    topController = topController.presentedViewController;
  [topController presentViewController:alert animated:YES completion:nil];
  self.visible = YES;
}

@end
