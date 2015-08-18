// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "config.h"
#import "platform/mac/VersionUtilMac.h"

#include <AppKit/AppKit.h>
#include <gtest/gtest.h>

#ifndef NSAppKitVersionNumber10_7
#define NSAppKitVersionNumber10_7 1138
#endif

#ifndef NSAppKitVersionNumber10_9
#define NSAppKitVersionNumber10_9 1265
#endif

#ifndef NSAppKitVersionNumber10_10
#define NSAppKitVersionNumber10_10 1343
#endif

// This number was determined by writing a tiny Cocoa App on 10.10.4.
#define NSAppKitVersionNumber10_10Max 1348

// AppKit version is loosely correlated to OSX version. It's still useful as a
// sanity check in unit tests, though we don't want to rely on it in production
// code.
TEST(VersionUtilMac, AppKitVersions)
{
    if (floor(NSAppKitVersionNumber) <= NSAppKitVersionNumber10_7) {
        EXPECT_TRUE(blink::IsOSLionOrEarlier());
        EXPECT_FALSE(blink::IsOSMavericksOrEarlier());
        EXPECT_FALSE(blink::IsOSMavericks());
        return;
    }

    if (floor(NSAppKitVersionNumber) < NSAppKitVersionNumber10_9) {
        EXPECT_FALSE(blink::IsOSLionOrEarlier());
        EXPECT_TRUE(blink::IsOSMavericksOrEarlier());
        EXPECT_FALSE(blink::IsOSMavericks());
        return;
    }
    if (floor(NSAppKitVersionNumber) == NSAppKitVersionNumber10_9) {
        EXPECT_FALSE(blink::IsOSLionOrEarlier());
        EXPECT_TRUE(blink::IsOSMavericksOrEarlier());
        EXPECT_TRUE(blink::IsOSMavericks());
        return;
    }
    if (floor(NSAppKitVersionNumber) <= NSAppKitVersionNumber10_10Max &&
        floor(NSAppKitVersionNumber) >=  NSAppKitVersionNumber10_10) {
        EXPECT_FALSE(blink::IsOSLionOrEarlier());
        EXPECT_FALSE(blink::IsOSMavericksOrEarlier());
        EXPECT_FALSE(blink::IsOSMavericks());
        return;
    }
}
