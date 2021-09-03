// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHOWCASE_FIRST_RUN_SC_FIRST_RUN_HERO_SCREEN_VIEW_CONTROLLER_H_
#define IOS_SHOWCASE_FIRST_RUN_SC_FIRST_RUN_HERO_SCREEN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/first_run/first_run_screen_view_controller.h"
#import "ios/chrome/browser/ui/first_run/first_run_screen_view_controller_delegate.h"

// Extends the base delegate protocol to handle taps on the custom button.
@protocol HeroScreenDelegate <FirstRunScreenViewControllerDelegate>

// Invoked when the custom action button is tapped.
- (void)didTapCustomActionButton;

@end

// A view controller to showcase an example hero screen for the new first run
// experience.
@interface SCFirstRunHeroScreenViewController : FirstRunScreenViewController

@property(nonatomic, weak) id<HeroScreenDelegate> delegate;

@end

#endif  // IOS_SHOWCASE_FIRST_RUN_SC_FIRST_RUN_HERO_SCREEN_VIEW_CONTROLLER_H_
