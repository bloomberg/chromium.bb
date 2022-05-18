// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"

#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ToolbarConfiguration

@synthesize style = _style;

- (instancetype)initWithStyle:(ToolbarStyle)style {
  self = [super init];
  if (self) {
    _style = style;
  }
  return self;
}

- (UIColor*)NTPBackgroundColor {
  return ntp_home::kNTPBackgroundColor();
}

- (UIColor*)backgroundColor {
  return [UIColor colorNamed:kBackgroundColor];
}

- (UIColor*)focusedBackgroundColor {
  return [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
}

- (UIColor*)focusedLocationBarBackgroundColor {
  return [UIColor colorNamed:kTextfieldFocusedBackgroundColor];
}

- (UIColor*)buttonsTintColor {
  return [UIColor colorNamed:kToolbarButtonColor];
}

- (UIColor*)buttonsTintColorHighlighted {
  return [UIColor colorNamed:@"tab_toolbar_button_color_highlighted"];
}

- (UIColor*)buttonsSpotlightColor {
  return [UIColor colorNamed:@"tab_toolbar_button_halo_color"];
}

- (UIColor*)dimmedButtonsSpotlightColor {
  return [UIColor colorNamed:@"tab_toolbar_button_halo_color"];
}

- (UIColor*)locationBarBackgroundColorWithVisibility:(CGFloat)visibilityFactor {
  // For the omnibox specifically, the background should be different in
  // incognito compared to dark mode.
  switch (self.style) {
    case NORMAL:
      return [[UIColor colorNamed:kTextfieldBackgroundColor]
          colorWithAlphaComponent:visibilityFactor];
    case INCOGNITO:
      return [[UIColor colorNamed:@"omnibox_incognito_background_color"]
          colorWithAlphaComponent:visibilityFactor];
  }
}

@end
