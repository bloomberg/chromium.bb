// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_CONFIRMATION_ALERT_CONFIRMATION_ALERT_VIEW_CONTROLLER_H_
#define IOS_CHROME_COMMON_UI_CONFIRMATION_ALERT_CONFIRMATION_ALERT_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// A11y Identifiers for testing.
extern NSString* const kConfirmationAlertMoreInfoAccessibilityIdentifier;
extern NSString* const kConfirmationAlertTitleAccessibilityIdentifier;
extern NSString* const kConfirmationAlertSubtitleAccessibilityIdentifier;
extern NSString* const kConfirmationAlertPrimaryActionAccessibilityIdentifier;
extern NSString* const kConfirmationAlertSecondaryActionAccessibilityIdentifier;
extern NSString* const
    kConfirmationAlertBarPrimaryActionAccessibilityIdentifier;

@protocol ConfirmationAlertActionHandler;

// A view controller useful to show modal alerts and confirmations. The main
// content consists in a big image, a title, and a subtitle which are contained
// in a scroll view for cases when the content doesn't fit in the screen.
// The view controller can have up to three action buttons, which are position
// in the bottom. They are arranged, from top to bottom,
// |primaryActionString|, |secondaryActionString|, |tertiaryActionString|.
// Setting those properties will make those buttons be added to the view
// controller.
@interface ConfirmationAlertViewController : UIViewController

// The headline below the image. Must be set before the view is loaded.
@property(nonatomic, copy) NSString* titleString;

// Text style for the title. If nil, will default to UIFontTextStyleTitle1.
@property(nonatomic, copy) NSString* titleTextStyle;

// The subtitle below the title. Must be set before the view is loaded.
@property(nonatomic, copy) NSString* subtitleString;

// The text for the primary action. Must be set before the view is loaded.
@property(nonatomic, copy) NSString* primaryActionString;

// The text for the secondary action. Must be set before the view is loaded.
@property(nonatomic, copy) NSString* secondaryActionString;

// The text for the tertiary action. Must be set before the view is loaded.
@property(nonatomic, copy) NSString* tertiaryActionString;

// The image. Must be set before the view is loaded.
@property(nonatomic, strong) UIImage* image;

// Sets the custom spacing between the top and the image, if there is no
// toolbar. Must be set before the view is loaded.
@property(nonatomic, assign) CGFloat customSpacingBeforeImageIfNoToolbar;

// Sets the custom spacing between the image and the title / subtitle. Must be
// set before the view is loaded.
@property(nonatomic, assign) CGFloat customSpacingAfterImage;

// When YES, the content is attached to the top of the view instead of being
// centered.
@property(nonatomic) BOOL topAlignedLayout;

// Value to determine whether or not the image's size should be scaled.
@property(nonatomic) BOOL imageHasFixedSize;

// Controls if there is a help button in the view. Must be set before the
// view is loaded.
@property(nonatomic) BOOL helpButtonAvailable;

// When set, this value will be set as the accessibility label for the help
// button.
@property(nonatomic, copy) NSString* helpButtonAccessibilityLabel;

// The help button item in the top left of the view. Nil if not available.
@property(nonatomic, readonly) UIBarButtonItem* helpButton;

// Controls if the toolbar dismiss button is available in the view. Default is
// YES. Must be set before the view is loaded.
@property(nonatomic) BOOL showDismissBarButton;

// Allows to modify the system item for the dismiss bar button (defaults to
// UIBarButtonSystemItemDone). Must be set before the view is loaded.
@property(nonatomic, assign) UIBarButtonSystemItem dismissBarButtonSystemItem;

// The action handler for interactions in this View Controller.
@property(nonatomic, weak) id<ConfirmationAlertActionHandler> actionHandler;

// Layout guide to specific content added in derived view controller.
@property(nonatomic, strong, readonly)
    UILayoutGuide* specificContentLayoutGuide;

// The container view for the screen-specific content. Derived view controllers
// should add their UI elements to it after -viewDidLoad. This view contains the
// confirmation alert contents, use |specificContentLayoutGuide| for set
// constraints below the contents. See crbug.com/1282434 for more.
@property(nonatomic, strong) UIView* specificContentSuperview;

@end

#endif  // IOS_CHROME_COMMON_UI_CONFIRMATION_ALERT_CONFIRMATION_ALERT_VIEW_CONTROLLER_H_
