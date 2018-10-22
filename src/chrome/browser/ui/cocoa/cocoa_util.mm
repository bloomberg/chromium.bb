// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/cocoa_util.h"

namespace cocoa_util {

void RemoveUnderlining(NSTextView* textView, int offset, int length) {
  // Clear the default link attributes that were set by the
  // HyperlinkTextView, otherwise removing the underline doesn't matter.
  [textView setLinkTextAttributes:nil];
  NSTextStorage* text = [textView textStorage];
  NSRange range = NSMakeRange(offset, length);
  [text addAttribute:NSUnderlineStyleAttributeName
               value:[NSNumber numberWithInt:NSUnderlineStyleNone]
               range:range];
}

CGFloat LineWidthFromContext(CGContextRef context) {
  CGRect unitRect = CGRectMake(0.0, 0.0, 1.0, 1.0);
  CGRect deviceRect = CGContextConvertRectToDeviceSpace(context, unitRect);
  return 1.0 / deviceRect.size.height;
}

}  // namespace cocoa_util
