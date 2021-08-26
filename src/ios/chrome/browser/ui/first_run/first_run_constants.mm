// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/first_run_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace first_run {

NSString* const kUMAMetricsButtonAccessibilityIdentifier =
    @"UMAMetricsButtonAccessibilityIdentifier";

NSString* const kFirstRunWelcomeScreenAccessibilityIdentifier =
    @"firstRunWelcomeScreenAccessibilityIdentifier";

NSString* const kFirstRunSignInScreenAccessibilityIdentifier =
    @"firstRunSignInScreenAccessibilityIdentifier";

NSString* const kFirstRunSyncScreenAccessibilityIdentifier =
    @"firstRunSyncScreenAccessibilityIdentifier";

NSString* const kFirstRunDefaultBrowserScreenAccessibilityIdentifier =
    @"firstRunDefaultBrowserScreenAccessibilityIdentifier";

NSString* const kBeginBoldTag = @"BEGIN_BOLD[ \t]*";
NSString* const kEndBoldTag = @"[ \t]*END_BOLD";

}  // first_run
