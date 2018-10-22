// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_util.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/foundation_util.h"

namespace bookmarks {
namespace bookmark_bar_util {

// Find something like std::is_between<T>?  I can't believe one doesn't exist.
BOOL ValueInRangeInclusive(CGFloat low, CGFloat value, CGFloat high) {
  return ((value >= low) && (value <= high));
}

}  // namespace bookmark_bar_util
}  // namespace bookmarks
