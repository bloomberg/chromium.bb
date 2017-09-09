// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_QUARTZCORE_ADDITIONS_H_
#define UI_BASE_COCOA_QUARTZCORE_ADDITIONS_H_

#import <QuartzCore/QuartzCore.h>

@interface CABasicAnimation (CrAdditions)
+ (instancetype)cr_animationWithKeyPath:(NSString*)path
                              fromValue:(id)fromValue
                                toValue:(id)toValue;
@end

#endif  // UI_BASE_COCOA_QUARTZCORE_ADDITIONS_H_
