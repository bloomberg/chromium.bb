// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_IOS_CLIENT_GESTURES_H_
#define REMOTING_CLIENT_IOS_CLIENT_GESTURES_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#include "base/logging.h"
#import "remoting/client/ios/key_input.h"

typedef NS_ENUM(NSInteger, HostInputScheme) {
  // Mouse cursor is shown
  // Dragging or Panning, moves the mouse cursor
  // Tapping causes mouse input at the cursor location
  HostInputSchemeTrackpad = 0,  // Default
  // Mouse cursor is not shown
  // Dragging or Panning is similar to a map
  // Tapping causes mouse input at the tap location
  HostInputSchemeTouch = 1
};

typedef NS_ENUM(NSInteger, MouseButton) {

  NO_BUTTON = 0,
  LEFT_BUTTON = 1,
  MIDDLE_BUTTON = 2,
  RIGHT_BUTTON = 3,

};

@interface ClientGestures : NSObject<UIGestureRecognizerDelegate> {
 @private
  UILongPressGestureRecognizer* _longPressRecognizer;
  UIPanGestureRecognizer* _panRecognizer;
  UIPanGestureRecognizer* _threeFingerPanRecognizer;
  UIPinchGestureRecognizer* _pinchRecognizer;
  UITapGestureRecognizer* _singleTapRecognizer;
  UITapGestureRecognizer* _twoFingerTapRecognizer;
  UITapGestureRecognizer* _threeFingerTapRecognizer;
  UIScreenEdgePanGestureRecognizer* _edgeGesture;
  UISwipeGestureRecognizer* _swipeGesture;

  HostInputScheme _inputScheme;
}

- (instancetype)initWithView:(UIView*)view;

// Zoom in/out
- (IBAction)pinchGestureTriggered:(UIPinchGestureRecognizer*)sender;
// Left mouse click, moves cursor
- (IBAction)tapGestureTriggered:(UITapGestureRecognizer*)sender;
// Scroll the view in 2d
- (IBAction)panGestureTriggered:(UIPanGestureRecognizer*)sender;
// Right mouse click and drag, moves cursor
- (IBAction)longPressGestureTriggered:(UILongPressGestureRecognizer*)sender;
// Right mouse click
- (IBAction)twoFingerTapGestureTriggered:(UITapGestureRecognizer*)sender;
// Middle mouse click
- (IBAction)threeFingerTapGestureTriggered:(UITapGestureRecognizer*)sender;
// Show hidden menus.  Swipe up for keyboard, swipe down for navigation menu
- (IBAction)threeFingerPanGestureTriggered:(UIPanGestureRecognizer*)sender;

@end

#endif  //  REMOTING_CLIENT_IOS_CLIENT_GESTURES_H_
