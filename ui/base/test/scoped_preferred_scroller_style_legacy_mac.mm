// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/test/scoped_preferred_scroller_style_legacy_mac.h"

#import "base/mac/sdk_forward_declarations.h"
#import "base/mac/scoped_objc_class_swizzler.h"

using base::mac::ScopedObjCClassSwizzler;

namespace {

void NotifyStyleChanged() {
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSPreferredScrollerStyleDidChangeNotification
                    object:nil];
}

}  // namespace

// Donates a testing implementation of +[NSScroller preferredScrollerStyle].
@interface FakeNSScrollerPreferredStyleDonor : NSObject
@end

@implementation FakeNSScrollerPreferredStyleDonor

+ (NSInteger)preferredScrollerStyle {
  return NSScrollerStyleLegacy;
}

@end

namespace ui {
namespace test {

ScopedPreferredScrollerStyleLegacy::ScopedPreferredScrollerStyleLegacy() {
  if (![NSScroller respondsToSelector:@selector(preferredScrollerStyle)])
    return;

  NSInteger previous_style = [NSScroller preferredScrollerStyle];

  swizzler_.reset(
      new ScopedObjCClassSwizzler([NSScroller class],
                                  [FakeNSScrollerPreferredStyleDonor class],
                                  @selector(preferredScrollerStyle)));

  if (previous_style != NSScrollerStyleLegacy)
    NotifyStyleChanged();
}

ScopedPreferredScrollerStyleLegacy::~ScopedPreferredScrollerStyleLegacy() {
  if (!swizzler_)
    return;  // Handle 10.6, which wouldn't have swizzled anything.

  swizzler_.reset();

  if ([NSScroller preferredScrollerStyle] != NSScrollerStyleLegacy)
    NotifyStyleChanged();
}

}  // namespace test
}  // namespace ui
