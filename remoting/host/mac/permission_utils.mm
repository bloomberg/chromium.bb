// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/host/mac/permission_utils.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"

namespace remoting {
namespace mac {

void PromptUserToChangeTrustStateIfNeeded() {
  // This method will only display a permission prompt if the application is
  // not trusted.
  NSDictionary* options = @{(NSString*)kAXTrustedCheckOptionPrompt : @YES };
  if (!AXIsProcessTrustedWithOptions((CFDictionaryRef)options)) {
    LOG(WARNING) << "AXIsProcessTrustedWithOptions: App is not trusted";
  }
}

}  // namespace mac
}  // namespace remoting
