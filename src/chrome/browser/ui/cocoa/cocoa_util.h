// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_COCOA_UTIL_H_
#define CHROME_BROWSER_UI_COCOA_COCOA_UTIL_H_

#import <Cocoa/Cocoa.h>

#include <limits>

#include "base/mac/foundation_util.h"

namespace cocoa_util {

// The minimum representable time interval.  This can be used as the value
// passed to +[NSAnimationContext setDuration:] to stop an in-progress
// animation as quickly as possible.
const NSTimeInterval kMinimumTimeInterval =
    std::numeric_limits<NSTimeInterval>::min();

// Remove underlining from the specified range of characters in a text view.
void RemoveUnderlining(NSTextView* textView, int offset, int length);

CGFloat LineWidthFromContext(CGContextRef context);

}  // namespace cocoa_util

#endif  // CHROME_BROWSER_UI_COCOA_COCOA_UTIL_H_
