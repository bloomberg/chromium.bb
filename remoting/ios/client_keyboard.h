// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_IOS_CLIENT_KEYBOARD_H_
#define REMOTING_CLIENT_IOS_CLIENT_KEYBOARD_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@interface ClientKeyboard : UIView<UIKeyInput, UITextInputTraits>

@property(nonatomic) UIKeyboardAppearance keyboardAppearance;
@property(nonatomic) UIKeyboardType keyboardType;
@property(nonatomic) UITextAutocapitalizationType autocapitalizationType;
@property(nonatomic) UITextAutocorrectionType autocorrectionType;
@property(nonatomic) UITextSpellCheckingType spellCheckingType;

@end

#endif  //  REMOTING_CLIENT_IOS_CLIENT_KEYBOARD_H_
