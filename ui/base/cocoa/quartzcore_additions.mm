// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/quartzcore_additions.h"

@implementation CABasicAnimation (CrAdditions)
+ (instancetype)cr_animationWithKeyPath:(nullable NSString*)path
                              fromValue:(id)fromValue
                                toValue:(id)toValue {
  CABasicAnimation* anim = [CABasicAnimation animationWithKeyPath:path];
  anim.fromValue = fromValue;
  anim.toValue = toValue;
  return anim;
}
@end
