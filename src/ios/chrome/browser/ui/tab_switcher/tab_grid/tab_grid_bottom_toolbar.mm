// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_bottom_toolbar.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/icons/chrome_symbol.h"
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
  UIBarButtonItem* _editButton;
  UIBarButtonItem* _addToButton;
  UIBarButtonItem* _closeTabsButton;
  UIBarButtonItem* _shareButton;
  BOOL _undoActive;
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
  if ((self.traitCollection.verticalSizeClass !=
       previousTraitCollection.verticalSizeClass) ||
      (self.traitCollection.horizontalSizeClass !=
       previousTraitCollection.horizontalSizeClass)) {
    [self updateLayout];
  }
}

// `pointInside` is called as long as this view is on the screen (even if its
// size is zero). It controls hit testing of the bottom toolbar. When the
// toolbar is transparent and has the `_largeNewTabButton`, only respond to
// tapping on that button.
- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent*)event {
  if ([self isShowingFloatingButton]) {
    // Only floating new tab button is tappable.
    return [_largeNewTabButton
        pointInside:[self convertPoint:point toView:_largeNewTabButton]
          withEvent:event];
  }
  return [super pointInside:point withEvent:event];
}

// Returns intrinsicContentSize based on the content of the toolbar.
// When showing the floating Button the contentsize for the toolbar should be
// zero so that the toolbar isn't accounted for when calculating the bottom
// insets of the container view.
- (CGSize)intrinsicContentSize {
  if ([self isShowingFloatingButton] || self.subviews.count == 0) {
    return CGSizeZero;
  }
  return _toolbar.intrinsicContentSize;
}

#pragma mark - Public

- (void)setPage:(TabGridPage)page {
  if (_page == page)
    return;
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
    // Setting the `accessibilityIdentifier` seems to trigger layout, which
    // causes an infinite loop.
    if (_closeAllOrUndoButton.accessibilityIdentifier !=
        kTabGridUndoCloseAllButtonIdentifier) {
      _closeAllOrUndoButton.accessibilityIdentifier =
          kTabGridUndoCloseAllButtonIdentifier;
    }
  } else {
    _closeAllOrUndoButton.title =
        l10n_util::GetNSString(IDS_IOS_TAB_GRID_CLOSE_ALL_BUTTON);
    // Setting the `accessibilityIdentifier` seems to trigger layout, which
    // causes an infinite loop.
    if (_closeAllOrUndoButton.accessibilityIdentifier !=
        kTabGridCloseAllButtonIdentifier) {
      _closeAllOrUndoButton.accessibilityIdentifier =
          kTabGridCloseAllButtonIdentifier;
    }
  }
  if (_undoActive != useUndo) {
    _undoActive = useUndo;
    [self updateLayout];
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

#pragma mark Edit Button

- (void)setEditButtonMenu:(UIMenu*)menu API_AVAILABLE(ios(14.0)) {
  _editButton.menu = menu;
}

- (void)setEditButtonEnabled:(BOOL)enabled {
  _editButton.enabled = enabled;
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

  if (UseSymbols()) {
    _smallNewTabButton = [[TabGridNewTabButton alloc] init];
  } else {
    _smallNewTabButton = [[TabGridNewTabButton alloc]
        initWithRegularImage:[UIImage imageNamed:@"new_tab_toolbar_button"]
              incognitoImage:
                  [UIImage imageNamed:@"new_tab_toolbar_button_incognito"]];
  }

  _smallNewTabButton.translatesAutoresizingMaskIntoConstraints = NO;
  _smallNewTabButton.page = self.page;

  _newTabButtonItem =
      [[UIBarButtonItem alloc] initWithCustomView:_smallNewTabButton];

  // Create selection mode buttons
  _editButton = [[UIBarButtonItem alloc] init];
  _editButton.tintColor = UIColorFromRGB(kTabGridToolbarTextButtonColor);
  _editButton.title = l10n_util::GetNSString(IDS_IOS_TAB_GRID_EDIT_BUTTON);
  _editButton.accessibilityIdentifier = kTabGridEditButtonIdentifier;

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

  _compactConstraints = @[
    [_toolbar.topAnchor constraintEqualToAnchor:self.topAnchor],
    [_toolbar.bottomAnchor
        constraintEqualToAnchor:self.safeAreaLayoutGuide.bottomAnchor],
    [_toolbar.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [_toolbar.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
  ];

  // For other layout, display a floating new tab button.
  if (UseSymbols()) {
    _largeNewTabButton = [[TabGridNewTabButton alloc] init];
  } else {
    UIImage* incognitoImage =
        [UIImage imageNamed:@"new_tab_floating_button_incognito"];
    _largeNewTabButton = [[TabGridNewTabButton alloc]
        initWithRegularImage:[UIImage imageNamed:@"new_tab_floating_button"]
              incognitoImage:incognitoImage];

    // When a11y font size is used, long press on UIBarButtonItem will show a
    // built-in a11y modal panel with image and title if set. The image will be
    // normalized into a bi-color image, so the incognito image is suitable
    // because it has a transparent "+". Use the larger image for higher
    // resolution.
    _newTabButtonItem.image = incognitoImage;
  }
  _largeNewTabButton.translatesAutoresizingMaskIntoConstraints = NO;
  _largeNewTabButton.page = self.page;

  CGFloat floatingButtonVerticalInset = kTabGridFloatingButtonVerticalInset;

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

  _newTabButtonItem.title = _largeNewTabButton.accessibilityLabel;
}

- (void)updateCloseTabsButtonTitle {
  _closeTabsButton.title = l10n_util::GetPluralNSStringF(
      IDS_IOS_TAB_GRID_CLOSE_TABS_BUTTON, _selectedTabsCount);
}

- (void)updateLayout {
  // Search mode doesn't have bottom toolbar or floating buttons, Handle it and
  // return early in that case.
  if (self.mode == TabGridModeSearch) {
    [NSLayoutConstraint deactivateConstraints:_compactConstraints];
    [NSLayoutConstraint deactivateConstraints:_floatingConstraints];
    [_toolbar removeFromSuperview];
    [_largeNewTabButton removeFromSuperview];
    self.hidden = !self.subviews.count;
    return;
  }
  _largeNewTabButtonBottomAnchor.constant =
      -kTabGridFloatingButtonVerticalInset;

  if (self.mode == TabGridModeSelection) {
    [NSLayoutConstraint deactivateConstraints:_floatingConstraints];
    [_largeNewTabButton removeFromSuperview];
    [_toolbar setItems:@[
      _closeTabsButton, _spaceItem, _shareButton, _spaceItem, _addToButton
    ]];
    [self addSubview:_toolbar];
    [NSLayoutConstraint activateConstraints:_compactConstraints];
    self.hidden = !self.subviews.count;
    return;
  }
  UIBarButtonItem* leadingButton = _closeAllOrUndoButton;
  if (!_undoActive)
    leadingButton = _editButton;
  UIBarButtonItem* trailingButton = _doneButton;

  if ([self shouldUseCompactLayout]) {
    [NSLayoutConstraint deactivateConstraints:_floatingConstraints];
    [_largeNewTabButton removeFromSuperview];

    // For incognito/regular pages, display all 3 buttons;
    // For remote tabs page, only display new tab button.
    if (self.page == TabGridPageRemoteTabs) {
      [_toolbar setItems:@[ _spaceItem, trailingButton ]];
    } else {
      [_toolbar setItems:@[
        leadingButton, _spaceItem, _newTabButtonItem, _spaceItem, trailingButton
      ]];
    }

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
  self.hidden = !self.subviews.count;
}

// Returns YES if the `_largeNewTabButton` is showing on the toolbar.
- (BOOL)isShowingFloatingButton {
  return _largeNewTabButton.superview &&
         _largeNewTabButtonBottomAnchor.isActive;
}

// Returns YES if should use compact bottom toolbar layout.
- (BOOL)shouldUseCompactLayout {
  return self.traitCollection.verticalSizeClass ==
             UIUserInterfaceSizeClassRegular &&
         self.traitCollection.horizontalSizeClass ==
             UIUserInterfaceSizeClassCompact;
}

@end
