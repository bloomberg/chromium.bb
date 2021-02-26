// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_GESTURES_VIEW_REVEALING_VERTICAL_PAN_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_GESTURES_VIEW_REVEALING_VERTICAL_PAN_HANDLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/gestures/layout_switcher_provider.h"
#import "ios/chrome/browser/ui/gestures/view_revealing_animatee.h"

// Responsible for handling vertical pan gestures to reveal/hide a view behind
// another.
// TODO(crbug.com/1123512): Add support for going straight from a Hidden state
// to a revealed state (and vice-versa) if the gesture's translation and
// velocity are enough to trigger such transition.
@interface ViewRevealingVerticalPanHandler : NSObject

// |peekedHeight| is the height of the view when peeked (partially revealed).
// |revealedCoverHeight| is the height of the cover view that remains visible
// after the view is revealed. |baseViewHeight| is the height of the base view.
- (instancetype)initWithPeekedHeight:(CGFloat)peekedHeight
                 revealedCoverHeight:(CGFloat)revealedCoverHeight
                      baseViewHeight:(CGFloat)baseViewHeight
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// When creating a pan gesture, this method is passed as the action on the init
// method. It is called when a gesture starts, moves, or ends.
- (void)handlePanGesture:(UIPanGestureRecognizer*)gesture;

// Adds UI element to the set of animated objects. The set holds weak references
// to the animatees.
- (void)addAnimatee:(id<ViewRevealingAnimatee>)animatee;

// Manually sets the state of the pan handler to a specific state, optionally
// animated.
- (void)setState:(ViewRevealState)state animated:(BOOL)animated;

// Height of the view that will be revealed after the transition to Peeked
// state.
@property(nonatomic, assign, readonly) CGFloat peekedHeight;
// Height of the revealed view after the transition to Revealed state.
@property(nonatomic, assign, readonly) CGFloat revealedHeight;
// Height of the base view. It changes when the user rotates the screen.
@property(nonatomic, assign) CGFloat baseViewHeight;
// The provider for the object that switches the layout of the revealed view
// from horizontal (Peeked state) to full (Revealed state).
@property(nonatomic, weak) id<LayoutSwitcherProvider> layoutSwitcherProvider;

@end

#endif  // IOS_CHROME_BROWSER_UI_GESTURES_VIEW_REVEALING_VERTICAL_PAN_HANDLER_H_
