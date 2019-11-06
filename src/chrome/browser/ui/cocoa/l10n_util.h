// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_L10N_UTIL_H_
#define CHROME_BROWSER_UI_COCOA_L10N_UTIL_H_

#import <Cocoa/Cocoa.h>
#include <stddef.h>

#include "base/strings/string16.h"

namespace cocoa_l10n_util {

// Compare function for -[NSArray sortedArrayUsingFunction:context:] that
// sorts the views in Y order bottom up. |context| is ignored.
NSInteger CompareFrameY(id view1, id view2, void* context);

// Helper for tweaking views. If view is a:
//   checkbox, radio group or label: it gets a forced wrap at current size
//   editable field: left as is
//   anything else: do  +[GTMUILocalizerAndLayoutTweaker sizeToFitView:]
NSSize WrapOrSizeToFit(NSView* view);

// Walks views in top-down order, wraps each to their current width, and moves
// the latter ones down to prevent overlaps. Returns the vertical delta in view
// coordinates.
CGFloat VerticallyReflowGroup(NSArray* views);

// Generates a tooltip string for a given URL and title.
NSString* TooltipForURLAndTitle(NSString* url, NSString* title);

// Set or clear the keys in NSUserDefaults which control UI direction based on
// whether direction is forced by a Chrome flag. This should be early in
// Chrome's launch, before any views or windows have been created, because it's
// cached by AppKit.
void ApplyForcedRTL();

}  // namespace cocoa_l10n_util

#endif  // CHROME_BROWSER_UI_COCOA_L10N_UTIL_H_
