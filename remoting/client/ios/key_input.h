// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_IOS_KEY_INPUT_H_
#define REMOTING_CLIENT_IOS_KEY_INPUT_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

// Key codes are translated from the on screen keyboard to the scan codes
// needed for Chromoting input.  We don't have a good automated approach to do
// this.  Instead we have created a mapping manually via trial and error.  To
// support other keyboards in this context we would have to test and create a
// mapping for each keyboard manually.

// Contract to handle translated key presses from the on-screen keyboard to
// the format required for Chromoting keyboard input.
@protocol KeyInputDelegate<NSObject>

- (void)keyboardShown;

- (void)keyboardDismissed;

- (void)keyboardActionKeyCode:(uint32_t)keyPressed isKeyDown:(BOOL)keyDown;

@end

@interface KeyInput : UIView<UIKeyInput>

@property(weak, nonatomic) id<KeyInputDelegate> delegate;
@property(readonly) BOOL keyboardVisible;
@property(readonly) CGFloat keyboardHeight;

- (void)ctrlAltDel;

@end

#endif  // REMOTING_CLIENT_IOS_KEY_INPUT_H_
