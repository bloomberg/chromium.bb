// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_TRANSITIONS_GRID_TRANSITION_ANIMATION_LAYOUT_PROVIDING_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_TRANSITIONS_GRID_TRANSITION_ANIMATION_LAYOUT_PROVIDING_H_

#import <Foundation/Foundation.h>

@class GridTransitionLayout;

// Objects conforming to this protocol can provide information for the
// animation of the transitions from and to a grid.
@protocol GridTransitionAnimationLayoutProviding

// YES if the currently selected cell is visible in the grid.
@property(nonatomic, readonly, getter=isSelectedCellVisible)
    BOOL selectedCellVisible;

// Asks the provider for the information on the layout of the grid of cells,
// used in transition animations.
- (GridTransitionLayout*)transitionLayout;

// Asks the provider for the view to which the animation views should be added.
- (UIView*)animationViewsContainer;

// Asks the provider for the view (if any) that animation views should be added
// in front of when building an animated transition. It's an error if this
// view is not nil and isn't an immediate subview of the view returned by
// |-animationViewsContainer|
- (UIView*)animationViewsContainerBottomView;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_TRANSITIONS_GRID_TRANSITION_ANIMATION_LAYOUT_PROVIDING_H_
