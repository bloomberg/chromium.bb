// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_bottom_toolbar.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_new_tab_button.h"
#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_feature.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TabGridBottomToolbar {
  UIToolbar* _toolbar;
  UIBarButtonItem* _newTabButtonItem;
  UIBarButtonItem* _spaceItem;
  NSArray<NSLayoutConstraint*>* _compactConstraints;
  NSArray<NSLayoutConstraint*>* _floatingConstraints;
  NSLayoutConstraint* _largeNewTabButtonBottomAnchor;
  TabGridNewTabButton* _smallNewTabButton;
  TabGridNewTabButton* _largeNewTabButton;
  UIBarButtonItem* _doneButton;
  UIBarButtonItem* _closeAllOrUndoButton;
  UIBarButtonItem* _addToButton;
  UIBarButtonItem* _closeTabsButton;
  UIBarButtonItem* _shareButton;
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  // The first time this moves to a superview, perform the view setup.
  if (newSuperview && self.subviews.count == 0) {
    [self setupViews];
  }
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self updateLayout];
}

// Controls hit testing of the bottom toolbar. When the toolbar is transparent,
// only respond to tapping on the new tab button.
- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent*)event {
  if ([self shouldShowFullBar]) {
    return [super pointInside:point withEvent:event];
  }
  // Only floating new tab button is tappable.
  return [_largeNewTabButton pointInside:[self convertPoint:point
                                                     toView:_largeNewTabButton]
                               withEvent:event];
}

// Returns UIToolbar's intrinsicContentSize based on the orientation and the
// mode.
- (CGSize)intrinsicContentSize {
  if ([self shouldShowFullBar]) {
    return _toolbar.intrinsicContentSize;
  }
  // Return CGSizeZero for floating button layout.
  return CGSizeZero;
}

#pragma mark - Public

// TODO(crbug.com/929981): "traitCollectionDidChange:" method won't get called
// when the view is not displayed, and in that case the only chance
// TabGridBottomToolbar can update its layout is when the TabGrid sets its
// "page" property in the
// "viewWillTransitionToSize:withTransitionCoordinator:" method. An early
// return for "self.page == page" can be added here since iOS 13 where the bug
// is fixed in UIKit.
- (void)setPage:(TabGridPage)page {
  _page = page;
  _smallNewTabButton.page = page;
  _largeNewTabButton.page = page;
  // Reset the title of UIBarButtonItem to update the title in a11y modal panel.
  _newTabButtonItem.title = _largeNewTabButton.accessibilityLabel;
  [self updateLayout];
}

- (void)setMode:(TabGridMode)mode {
  if (_mode == mode)
    return;
  _mode = mode;
  // Reset selected tabs count when mode changes.
  self.selectedTabsCount = 0;
  [self updateLayout];
}

- (void)setSelectedTabsCount:(int)count {
  _selectedTabsCount = count;
  [self updateCloseTabsButtonTitle];
}

- (void)setNewTabButtonTarget:(id)target action:(SEL)action {
  [_smallNewTabButton addTarget:target
                         action:action
               forControlEvents:UIControlEventTouchUpInside];
  [_largeNewTabButton addTarget:target
                         action:action
               forControlEvents:UIControlEventTouchUpInside];
}

- (void)setCloseAllButtonTarget:(id)target action:(SEL)action {
  _closeAllOrUndoButton.target = target;
  _closeAllOrUndoButton.action = action;
}

- (void)setDoneButtonTarget:(id)target action:(SEL)action {
  _doneButton.target = target;
  _doneButton.action = action;
}

- (void)setNewTabButtonEnabled:(BOOL)enabled {
  _smallNewTabButton.enabled = enabled;
  _largeNewTabButton.enabled = enabled;
}

- (void)setDoneButtonEnabled:(BOOL)enabled {
  _doneButton.enabled = enabled;
}

- (void)setCloseAllButtonEnabled:(BOOL)enabled {
  _closeAllOrUndoButton.enabled = enabled;
}

- (void)useUndoCloseAll:(BOOL)useUndo {
  _closeAllOrUndoButton.enabled = YES;
  if (useUndo) {
    _closeAllOrUndoButton.title =
        l10n_util::GetNSString(IDS_IOS_TAB_GRID_UNDO_CLOSE_ALL_BUTTON);
    // Setting the |accessibilityIdentifier| seems to trigger layout, which
    // causes an infinite loop.
    if (_closeAllOrUndoButton.accessibilityIdentifier !=
        kTabGridUndoCloseAllButtonIdentifier) {
      _closeAllOrUndoButton.accessibilityIdentifier =
          kTabGridUndoCloseAllButtonIdentifier;
    }
  } else {
    _closeAllOrUndoButton.title =
        l10n_util::GetNSString(IDS_IOS_TAB_GRID_CLOSE_ALL_BUTTON);
    // Setting the |accessibilityIdentifier| seems to trigger layout, which
    // causes an infinite loop.
    if (_closeAllOrUndoButton.accessibilityIdentifier !=
        kTabGridCloseAllButtonIdentifier) {
      _closeAllOrUndoButton.accessibilityIdentifier =
          kTabGridCloseAllButtonIdentifier;
    }
  }
}

- (void)hide {
  _smallNewTabButton.alpha = 0.0;
  _largeNewTabButton.alpha = 0.0;
}

- (void)show {
  _smallNewTabButton.alpha = 1.0;
  _largeNewTabButton.alpha = 1.0;
}

#pragma mark Close Tabs

- (void)setCloseTabsButtonTarget:(id)target action:(SEL)action {
  _closeTabsButton.target = target;
  _closeTabsButton.action = action;
}

- (void)setCloseTabsButtonEnabled:(BOOL)enabled {
  _closeTabsButton.enabled = enabled;
}

#pragma mark Share Tabs

- (void)setShareTabsButtonTarget:(id)target action:(SEL)action {
  _shareButton.target = target;
  _shareButton.action = action;
}
- (void)setShareTabsButtonEnabled:(BOOL)enabled {
  _shareButton.enabled = enabled;
}

#pragma mark Add To

- (void)setAddToButtonMenu:(UIMenu*)menu API_AVAILABLE(ios(14.0)) {
  _addToButton.menu = menu;
}

- (void)setAddToButtonEnabled:(BOOL)enabled {
  _addToButton.enabled = enabled;
}

#pragma mark - Private

- (void)setupViews {
  // For Regular(V) x Compact(H) layout, display UIToolbar.
  // In iOS 13, constraints break if the UIToolbar is initialized with a null or
  // zero rect frame. An arbitrary non-zero frame fixes this issue.
  _toolbar = [[UIToolbar alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  _toolbar.translatesAutoresizingMaskIntoConstraints = NO;
  _toolbar.barStyle = UIBarStyleBlack;
  _toolbar.translucent = YES;
  // Remove the border of UIToolbar.
  [_toolbar setShadowImage:[[UIImage alloc] init]
        forToolbarPosition:UIBarPositionAny];

  _closeAllOrUndoButton = [[UIBarButtonItem alloc] init];
  _closeAllOrUndoButton.tintColor =
      UIColorFromRGB(kTabGridToolbarTextButtonColor);

  _doneButton = [[UIBarButtonItem alloc] init];
  _doneButton.style = UIBarButtonItemStyleDone;
  _doneButton.tintColor = UIColorFromRGB(kTabGridToolbarTextButtonColor);
  _doneButton.title = l10n_util::GetNSString(IDS_IOS_TAB_GRID_DONE_BUTTON);
  _doneButton.accessibilityIdentifier = kTabGridDoneButtonIdentifier;

  _spaceItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];

  _smallNewTabButton = [[TabGridNewTabButton alloc]
      initWithRegularImage:[UIImage imageNamed:@"new_tab_toolbar_button"]
            incognitoImage:[UIImage
                               imageNamed:@"new_tab_toolbar_button_incognito"]];
  _smallNewTabButton.translatesAutoresizingMaskIntoConstraints = NO;
  _smallNewTabButton.page = self.page;

  _newTabButtonItem =
      [[UIBarButtonItem alloc] initWithCustomView:_smallNewTabButton];

  // Create selection mode buttons
  if (IsTabsBulkActionsEnabled()) {
    _addToButton = [[UIBarButtonItem alloc] init];
    _addToButton.tintColor = UIColorFromRGB(kTabGridToolbarTextButtonColor);
    _addToButton.title = l10n_util::GetNSString(IDS_IOS_TAB_GRID_ADD_TO_BUTTON);
    _addToButton.accessibilityIdentifier = kTabGridEditAddToButtonIdentifier;
    _shareButton = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemAction
                             target:nil
                             action:nil];
    _shareButton.tintColor = UIColorFromRGB(kTabGridToolbarTextButtonColor);
    _shareButton.accessibilityIdentifier = kTabGridEditShareButtonIdentifier;
    _closeTabsButton = [[UIBarButtonItem alloc] init];
    _closeTabsButton.tintColor = UIColorFromRGB(kTabGridToolbarTextButtonColor);
    _closeTabsButton.accessibilityIdentifier =
        kTabGridEditCloseTabsButtonIdentifier;
    [self updateCloseTabsButtonTitle];
  }

  _compactConstraints = @[
    [_toolbar.topAnchor constraintEqualToAnchor:self.topAnchor],
    [_toolbar.bottomAnchor
        constraintEqualToAnchor:self.safeAreaLayoutGuide.bottomAnchor],
    [_toolbar.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [_toolbar.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
  ];

  // For other layout, display a floating new tab button.
  UIImage* incognitoImage =
      [UIImage imageNamed:@"new_tab_floating_button_incognito"];
  _largeNewTabButton = [[TabGridNewTabButton alloc]
      initWithRegularImage:[UIImage imageNamed:@"new_tab_floating_button"]
            incognitoImage:incognitoImage];
  _largeNewTabButton.translatesAutoresizingMaskIntoConstraints = NO;
  _largeNewTabButton.page = self.page;

  CGFloat floatingButtonVerticalInset = kTabGridFloatingButtonVerticalInset;
  if (ShowThumbStripInTraitCollection(self.traitCollection)) {
    floatingButtonVerticalInset += kBVCHeightTabGrid;
  }

  _largeNewTabButtonBottomAnchor = [_largeNewTabButton.bottomAnchor
      constraintEqualToAnchor:self.safeAreaLayoutGuide.bottomAnchor
                     constant:-floatingButtonVerticalInset];

  _floatingConstraints = @[
    [_largeNewTabButton.topAnchor constraintEqualToAnchor:self.topAnchor],
    _largeNewTabButtonBottomAnchor,
    [_largeNewTabButton.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor
                       constant:-kTabGridFloatingButtonHorizontalInset],
  ];

  // When a11y font size is used, long press on UIBarButtonItem will show a
  // built-in a11y modal panel with image and title if set. The image will be
  // normalized into a bi-color image, so the incognito image is suitable
  // because it has a transparent "+". Use the larger image for higher
  // resolution.
  _newTabButtonItem.image = incognitoImage;
  _newTabButtonItem.title = _largeNewTabButton.accessibilityLabel;
}

- (void)updateCloseTabsButtonTitle {
  _closeTabsButton.title = l10n_util::GetPluralNSStringF(
      IDS_IOS_TAB_GRID_CLOSE_TABS_BUTTON, _selectedTabsCount);
}

- (void)updateLayout {
  _largeNewTabButtonBottomAnchor.constant =
      -kTabGridFloatingButtonVerticalInset;

  if (self.mode == TabGridModeSelection) {
    [_toolbar setItems:@[
      _closeTabsButton, _spaceItem, _shareButton, _spaceItem, _addToButton
    ]];
    [NSLayoutConstraint deactivateConstraints:_floatingConstraints];
    [_largeNewTabButton removeFromSuperview];
    [self addSubview:_toolbar];
    [NSLayoutConstraint activateConstraints:_compactConstraints];
    return;
  }

  UIBarButtonItem* leadingButton = _closeAllOrUndoButton;
  UIBarButtonItem* trailingButton = _doneButton;

  if ([self shouldUseCompactLayout]) {
    // For incognito/regular pages, display all 3 buttons;
    // For remote tabs page, only display new tab button.
    if (self.page == TabGridPageRemoteTabs) {
      [_toolbar setItems:@[ _spaceItem, trailingButton ]];
    } else {
      [_toolbar setItems:@[
        leadingButton, _spaceItem, _newTabButtonItem, _spaceItem, trailingButton
      ]];
    }

    [NSLayoutConstraint deactivateConstraints:_floatingConstraints];
    [_largeNewTabButton removeFromSuperview];
    [self addSubview:_toolbar];
    [NSLayoutConstraint activateConstraints:_compactConstraints];
  } else {
    [NSLayoutConstraint deactivateConstraints:_compactConstraints];
    [_toolbar removeFromSuperview];

    // When the thumb strip is enabled, there should be no new tab button on the
    // bottom ever.
    if (ShowThumbStripInTraitCollection(self.traitCollection) ||
        self.page == TabGridPageRemoteTabs) {
      [NSLayoutConstraint deactivateConstraints:_floatingConstraints];
      [_largeNewTabButton removeFromSuperview];
    } else {
      [self addSubview:_largeNewTabButton];
      [NSLayoutConstraint activateConstraints:_floatingConstraints];
    }
  }
}

// Returns YES if the full toolbar should be shown instead of the floating
// button.
- (BOOL)shouldShowFullBar {
  return [self shouldUseCompactLayout] || self.mode == TabGridModeSelection;
}

// Returns YES if should use compact bottom toolbar layout.
- (BOOL)shouldUseCompactLayout {
  // TODO(crbug.com/929981): UIView's |traitCollection| can be wrong and
  // contradict the keyWindow's |traitCollection| because UIView's
  // |-traitCollectionDidChange:| is not properly called when the view rotates
  // while it is in a ViewController deeper in the ViewController hierarchy. Use
  // self.traitCollection since iOS 13 where the bug is fixed in UIKit.
  return self.window.traitCollection.verticalSizeClass ==
             UIUserInterfaceSizeClassRegular &&
         self.window.traitCollection.horizontalSizeClass ==
             UIUserInterfaceSizeClassCompact;
}

@end
