// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_IOS_APP_PIN_ENTRY_VIEW_CONTROLLER_H_
#define REMOTING_CLIENT_IOS_APP_PIN_ENTRY_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

typedef void (^PinEntryModalCallback)(NSString* hostPin, BOOL createPairing);

// PinEntryViewController is expected to be created, displayed and then
// destroyed after the pin entry is finished; not reused.
@interface PinEntryViewController : UIViewController

// Callback will be used to return the pin entry result.
- (id)initWithCallback:(PinEntryModalCallback)callback;

@end

#endif  // REMOTING_CLIENT_IOS_APP_PIN_ENTRY_VIEW_CONTROLLER_H_
