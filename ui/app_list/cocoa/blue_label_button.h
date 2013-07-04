// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_COCOA_BLUE_LABEL_BUTTON_H_
#define UI_APP_LIST_COCOA_BLUE_LABEL_BUTTON_H_

#import <Cocoa/Cocoa.h>

#include "ui/app_list/app_list_export.h"
#import "ui/base/cocoa/hover_button.h"

// A rectangular blue NSButton that reacts to hover, focus and lit states. It
// can contain an arbitrary single-line text label, and will be sized to fit the
// font height and label width.
APP_LIST_EXPORT
@interface BlueLabelButton : HoverButton
@end

#endif  // UI_APP_LIST_COCOA_BLUE_LABEL_BUTTON_H_
