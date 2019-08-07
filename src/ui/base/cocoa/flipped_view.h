// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_FLIPPED_VIEW_H_
#define UI_BASE_COCOA_FLIPPED_VIEW_H_

#import <Cocoa/Cocoa.h>

#include "ui/base/ui_base_export.h"

// A view where the Y axis is flipped such that the origin is at the top left
// and Y value increases downwards. Drawing is flipped so that layout of the
// sections is easier. Apple recommends flipping the coordinate origin when
// doing a lot of text layout because it's more natural.
UI_BASE_EXPORT
@interface FlippedView : NSView
@end

#endif  // UI_BASE_COCOA_FLIPPED_VIEW_H_
