// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "config.h"
#import "platform/mac/VersionUtilMac.h"

#import <AppKit/AppKit.h>

#ifndef NSAppKitVersionNumber10_9
#define NSAppKitVersionNumber10_9 1265
#endif

namespace blink {

bool IsOSMavericksOrEarlier()
{
    return floor(NSAppKitVersionNumber) <= NSAppKitVersionNumber10_9;
}

bool IsOSMavericks()
{
    return floor(NSAppKitVersionNumber) == NSAppKitVersionNumber10_9;
}

} // namespace blink
