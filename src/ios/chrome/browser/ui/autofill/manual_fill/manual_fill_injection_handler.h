// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_INJECTION_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_INJECTION_HANDLER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_content_delegate.h"

class WebStateList;

// Handler with the common logic for injecting data from manual fill.
@interface ManualFillInjectionHandler : NSObject<ManualFillContentDelegate>

// Returns a handler using the passed `WebStateList` in order to inject JS in
// the active web state.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_INJECTION_HANDLER_H_
