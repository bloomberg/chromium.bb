// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/key_input.h"
#import "remoting/ios/key_map_us.h"

@interface KeyInput (Private)
- (void)transmitAppropriateKeyCode:(NSString*)text;
- (void)transmitKeyCode:(NSInteger)keyCode needShift:(bool)needShift;
@end

@implementation KeyInput

@synthesize delegate = _delegate;

// Override UIKeyInput::UITextInputTraits property
- (UIKeyboardType)keyboardType {
  return UIKeyboardTypeAlphabet;
}

// Override UIView::UIResponder, when this interface is the first responder
// on-screen keyboard input will create events for Chromoting keyboard input
- (BOOL)canBecomeFirstResponder {
  return YES;
}

// Override UIView::UIResponder
// Keyboard was dismissed
- (BOOL)resignFirstResponder {
  BOOL wasFirstResponder = self.isFirstResponder;
  BOOL didResignFirstReponder =
      [super resignFirstResponder];  // I'm not sure that this returns YES when
                                     // first responder was resigned, but for
                                     // now I don't actually need to know what
                                     // the return from super means.
  if (wasFirstResponder) {
    [_delegate keyboardDismissed];
  }

  return didResignFirstReponder;
}

// @protocol UIKeyInput, Send backspace
- (void)deleteBackward {
  [self transmitKeyCode:kKeyCodeUS[kBackspaceIndex] needShift:false];
}

// @protocol UIKeyInput, Assume this is a text input
- (BOOL)hasText {
  return YES;
}

// @protocol UIKeyInput, Translate inserted text to key presses, one char at a
// time
- (void)insertText:(NSString*)text {
  [self transmitAppropriateKeyCode:text];
}

- (void)ctrlAltDel {
  if (_delegate) {
    [_delegate keyboardActionKeyCode:kKeyCodeUS[kCtrlIndex] isKeyDown:YES];
    [_delegate keyboardActionKeyCode:kKeyCodeUS[kAltIndex] isKeyDown:YES];
    [_delegate keyboardActionKeyCode:kKeyCodeUS[kDelIndex] isKeyDown:YES];
    [_delegate keyboardActionKeyCode:kKeyCodeUS[kDelIndex] isKeyDown:NO];
    [_delegate keyboardActionKeyCode:kKeyCodeUS[kAltIndex] isKeyDown:NO];
    [_delegate keyboardActionKeyCode:kKeyCodeUS[kCtrlIndex] isKeyDown:NO];
  }
}

// When inserting multiple characters, process them one at a time.  |text| is as
// it was output on the device.  The shift key is not naturally presented in the
// input stream, and must be inserted by inspecting each char and considering
// that if the key was input on a traditional keyboard that the character would
// have required a shift.  Assume caps lock does not exist.
- (void)transmitAppropriateKeyCode:(NSString*)text {
  for (int i = 0; i < [text length]; ++i) {
    NSInteger charToSend = [text characterAtIndex:i];

    if (charToSend <= kKeyboardKeyMaxUS) {
      [self transmitKeyCode:kKeyCodeUS[charToSend]
                  needShift:kIsShiftRequiredUS[charToSend]];
    }
  }
}

// |charToSend| is as it was output on the device.  Some call this a
// 'key press'.  For Chromoting this must be transferred as a key down (press
// down with a finger), followed by a key up (finger is removed from the
// keyboard)
//
// The delivery may be an upper case or special character.  Chromoting is just
// interested in the button that was pushed, so to create an upper case
// character, first send a shift press, then the button, then release shift
- (void)transmitKeyCode:(NSInteger)keyCode needShift:(bool)needShift {
  if (keyCode > 0 && _delegate) {
    if (needShift) {
      [_delegate keyboardActionKeyCode:kKeyCodeUS[kShiftIndex] isKeyDown:YES];
    }
    [_delegate keyboardActionKeyCode:keyCode isKeyDown:YES];
    [_delegate keyboardActionKeyCode:keyCode isKeyDown:NO];
    if (needShift) {
      [_delegate keyboardActionKeyCode:kKeyCodeUS[kShiftIndex] isKeyDown:NO];
    }
  }
}
@end
