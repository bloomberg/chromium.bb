// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_TRANSITIONS_REDUCED_MOTION_ANIMATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_TRANSITIONS_REDUCED_MOTION_ANIMATOR_H_

#import <UIKit/UIKit.h>

// Animator object used when a simpler transition is needed.
@interface ReducedMotionAnimator
    : NSObject<UIViewControllerAnimatedTransitioning>

// YES if the animation presents a tab; NO if it dismisses it.
@property(nonatomic, assign) BOOL presenting;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_TRANSITIONS_REDUCED_MOTION_ANIMATOR_H_
