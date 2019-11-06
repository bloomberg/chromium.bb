// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_new_tab_button.h"

#import "ios/chrome/browser/ui/tab_grid/tab_grid_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabGridNewTabButton () {
  UIView* _container;

  UIImage* _regularImage;
  UIImage* _incognitoImage;
  UIImage* _smallRegularImage;
  UIImage* _smallIncognitoImage;
  UIImage* _largeRegularImage;
  UIImage* _largeIncognitoImage;

  NSArray* _smallButtonConstraints;
  NSArray* _largeButtonConstraints;
}
@end

@implementation TabGridNewTabButton

- (instancetype)init {
  self = [super init];
  if (self) {
    _smallRegularImage = [UIImage imageNamed:@"new_tab_toolbar_button"];
    _smallIncognitoImage =
        [UIImage imageNamed:@"new_tab_toolbar_button_incognito"];
    _largeRegularImage = [UIImage imageNamed:@"new_tab_floating_button"];
    _largeIncognitoImage =
        [UIImage imageNamed:@"new_tab_floating_button_incognito"];
    // Set UIBarButtonItem.image to get a built-in accessibility modal panel.
    // The panel will be shown when user long press on the button, under
    // accessibility font size. The image will be normalized into a bi-color
    // image, so the incognito image is suitable because it has a transparent
    // "+". Use the larger image for higher resolution.
    self.image = _largeIncognitoImage;

    _container = [[UIView alloc] init];
    _container.translatesAutoresizingMaskIntoConstraints = NO;
    self.customView = _container;

    _button = [[UIButton alloc] init];
    _button.translatesAutoresizingMaskIntoConstraints = NO;
    // Set a high compression resistance priority otherwise the button will be
    // compressed by UIToolbar's height constraint.
    [_button
        setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh + 1
                                        forAxis:UILayoutConstraintAxisVertical];
    [_container addSubview:_button];

    _smallButtonConstraints = @[
      [_button.topAnchor constraintEqualToAnchor:_container.topAnchor],
      [_button.bottomAnchor constraintEqualToAnchor:_container.bottomAnchor],
      [_button.leadingAnchor constraintEqualToAnchor:_container.leadingAnchor],
      [_button.trailingAnchor constraintEqualToAnchor:_container.trailingAnchor]
    ];

    _largeButtonConstraints = @[
      [_button.topAnchor constraintEqualToAnchor:_container.topAnchor],
      [_button.bottomAnchor
          constraintEqualToAnchor:_container.bottomAnchor
                         constant:-kTabGridFloatingButtonVerticalInset],
      [_button.leadingAnchor constraintEqualToAnchor:_container.leadingAnchor],
      [_button.trailingAnchor constraintEqualToAnchor:_container.trailingAnchor]
    ];
  }
  return self;
}

#pragma mark - Public

- (void)setPage:(TabGridPage)page {
  if (page == _page)
    return;
  switch (page) {
    case TabGridPageIncognitoTabs:
      self.button.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TAB_GRID_CREATE_NEW_INCOGNITO_TAB);
      break;
    case TabGridPageRegularTabs:
      self.button.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TAB_GRID_CREATE_NEW_TAB);
      break;
    case TabGridPageRemoteTabs:
      break;
  }
  self.title = self.button.accessibilityLabel;
  _page = page;
  [self updateButtonImage];
}

- (void)setSizeClass:(TabGridNewTabButtonSizeClass)sizeClass {
  if (sizeClass == _sizeClass)
    return;
  switch (sizeClass) {
    case TabGridNewTabButtonSizeClassSmall:
      _regularImage = _smallRegularImage;
      _incognitoImage = _smallIncognitoImage;
      [NSLayoutConstraint deactivateConstraints:_largeButtonConstraints];
      [NSLayoutConstraint activateConstraints:_smallButtonConstraints];
      break;
    case TabGridNewTabButtonSizeClassLarge:
      _regularImage = _largeRegularImage;
      _incognitoImage = _largeIncognitoImage;
      [NSLayoutConstraint deactivateConstraints:_smallButtonConstraints];
      [NSLayoutConstraint activateConstraints:_largeButtonConstraints];
      break;
  }
  _sizeClass = sizeClass;
  [self updateButtonImage];
}

#pragma mark - Private

- (void)updateButtonImage {
  UIImage* renderedImage;
  switch (self.page) {
    case TabGridPageIncognitoTabs:
      renderedImage = [_incognitoImage
          imageWithRenderingMode:UIImageRenderingModeAlwaysOriginal];
      break;
    case TabGridPageRegularTabs:
      renderedImage = [_regularImage
          imageWithRenderingMode:UIImageRenderingModeAlwaysOriginal];
      break;
    default:
      break;
  }
  [self.button setImage:renderedImage forState:UIControlStateNormal];
}

@end
