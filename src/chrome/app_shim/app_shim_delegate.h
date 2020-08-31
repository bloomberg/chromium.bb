// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_SHIM_APP_SHIM_DELEGATE_H_
#define CHROME_APP_SHIM_APP_SHIM_DELEGATE_H_

#import <Cocoa/Cocoa.h>
#include <vector>

#include "base/files/file_path.h"

class AppShimController;

// An application delegate to catch user interactions and send the appropriate
// IPC messages to Chrome.
@interface AppShimDelegate
    : NSObject<NSApplicationDelegate, NSUserInterfaceValidations> {
 @private
  AppShimController* _appShimController;  // Weak, owns |this|
}
- (instancetype)initWithController:(AppShimController*)controller;
@end

#endif  // CHROME_APP_SHIM_APP_SHIM_DELEGATE_H_
