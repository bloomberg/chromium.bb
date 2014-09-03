// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_IOS_NSSTRING_CR_STRING_DRAWING_H_
#define UI_IOS_NSSTRING_CR_STRING_DRAWING_H_

#import <UIKit/UIKit.h>

@interface NSString (CrStringDrawing)

// Returns the size of the string if it were to be rendered with the specified
// font on a single line. The width and height of the CGSize returned are
// pixel-aligned.
//
// This method is a convenience wrapper around sizeWithAttributes: to avoid
// boilerplate required to put |font| in a dictionary of attributes. Do not pass
// nil into this method.
- (CGSize)cr_pixelAlignedSizeWithFont:(UIFont*)font;

// Deprecated: Use cr_pixelAlignedSizeWithFont: or sizeWithAttributes:
// Provides a drop-in replacement for sizeWithFont:, which was deprecated in iOS
// 7 in favor of -sizeWithAttributes:. Specifically, this method will return
// CGSizeZero if |font| is nil, and the width and height returned are rounded up
// to integer values.
// TODO(lliabraa): This method was added to ease the transition off of the
// deprecated sizeWithFont: method. New call sites should not be added and
// existing call sites should be audited to determine the correct behavior for
// nil |font| and rounding, then replaced with cr_pixelAlignedSizeWithFont: or
// sizeWithAttributes: (crbug.com/364419).
- (CGSize)cr_sizeWithFont:(UIFont*)font;

@end

#endif  // UI_IOS_NSSTRING_CR_STRING_DRAWING_H_
