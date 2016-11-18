// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/client/ios/key_input.h"
#import "remoting/client/ios/key_map_us.h"

@interface KeyInput (Private)
- (void)transmitAppropriateKeyCode:(NSString*)text;
- (void)transmitKeyCode:(NSInteger)keyCode needShift:(bool)needShift;
@end

@implementation KeyInput

@synthesize delegate = _delegate;
@synthesize keyboardVisible = _keyboardVisible;
@synthesize keyboardHeight = _keyboardHeight;

// Override UIView
- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center addObserver:self
               selector:@selector(keyboardDidShow:)
                   name:UIKeyboardDidShowNotification
                 object:nil];
    [center addObserver:self
               selector:@selector(keyboardWillHide:)
                   name:UIKeyboardWillHideNotification
                 object:nil];
  }
  return self;
}

// Override NSObject
- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

// Callback from NotificationCenter
- (void)keyboardDidShow:(NSNotification*)notification {
  NSDictionary* userInfo = [notification userInfo];
  CGSize keyboardSize =
      [[userInfo objectForKey:UIKeyboardFrameEndUserInfoKey] CGRectValue].size;
  _keyboardHeight = keyboardSize.height;
  _keyboardVisible = YES;
  [_delegate keyboardShown];
}

// Callback from NotificationCenter
- (void)keyboardWillHide:(NSNotification*)notification {
  _keyboardHeight = 0.0;
  _keyboardVisible = NO;
  [_delegate keyboardDismissed];
}

// Override UIKeyInput::UITextInputTraits property.
- (UIKeyboardType)keyboardType {
  return UIKeyboardTypeAlphabet;
}

// Override UIKeyInput::UITextInputTraits property.
// Remove text completion.
- (UITextAutocorrectionType)autocorrectionType {
  return UITextAutocorrectionTypeNo;
}

// Override UIView::UIResponder, when this interface is the first responder
// on-screen keyboard input will create events for Chromoting keyboard input.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

// @protocol UIKeyInput, Send backspace.
- (void)deleteBackward {
  [self transmitKeyCode:kKeyCodeMetaUS[kBackspaceIndex].code needShift:false];
}

// @protocol UIKeyInput, Assume this is a text input.
- (BOOL)hasText {
  return YES;
}

// @protocol UIKeyInput, Translate inserted text to key presses, one char at a
// time.
- (void)insertText:(NSString*)text {
  [self transmitAppropriateKeyCode:text];
}

- (void)ctrlAltDel {
  if (_delegate) {
    [_delegate keyboardActionKeyCode:kKeyCodeMetaUS[kCtrlIndex].code
                           isKeyDown:YES];
    [_delegate keyboardActionKeyCode:kKeyCodeMetaUS[kAltIndex].code
                           isKeyDown:YES];
    [_delegate keyboardActionKeyCode:kKeyCodeMetaUS[kDelIndex].code
                           isKeyDown:YES];
    [_delegate keyboardActionKeyCode:kKeyCodeMetaUS[kDelIndex].code
                           isKeyDown:NO];
    [_delegate keyboardActionKeyCode:kKeyCodeMetaUS[kAltIndex].code
                           isKeyDown:NO];
    [_delegate keyboardActionKeyCode:kKeyCodeMetaUS[kCtrlIndex].code
                           isKeyDown:NO];
  }
}

// When inserting multiple characters, process them one at a time.  |text| is as
// it was output on the device.  The shift key is not naturally presented in the
// input stream, and must be inserted by inspecting each char and considering
// that if the key was input on a traditional keyboard that the character would
// have required a shift.  Assume caps lock does not exist.
- (void)transmitAppropriateKeyCode:(NSString*)text {
  for (size_t i = 0; i < [text length]; ++i) {
    NSInteger charToSend = [text characterAtIndex:i];

    if (charToSend <= kKeyboardKeyMaxUS) {
      [self transmitKeyCode:kKeyCodeMetaUS[charToSend].code
                  needShift:kKeyCodeMetaUS[charToSend].needsShift];
    }
  }
}

// |charToSend| is as it was output on the device.  Some call this a
// 'key press'.  For Chromoting this must be transferred as a key down (press
// down with a finger), followed by a key up (finger is removed from the
// keyboard).
//
// The delivery may be an upper case or special character.  Chromoting is just
// interested in the button that was pushed, so to create an upper case
// character, first send a shift press, then the button, then release shift.
- (void)transmitKeyCode:(NSInteger)keyCode needShift:(bool)needShift {
  if (keyCode > 0 && _delegate) {
    if (needShift) {
      [_delegate keyboardActionKeyCode:kKeyCodeMetaUS[kShiftIndex].code
                             isKeyDown:YES];
    }
    [_delegate keyboardActionKeyCode:keyCode isKeyDown:YES];
    [_delegate keyboardActionKeyCode:keyCode isKeyDown:NO];
    if (needShift) {
      [_delegate keyboardActionKeyCode:kKeyCodeMetaUS[kShiftIndex].code
                             isKeyDown:NO];
    }
  }
}
@end
