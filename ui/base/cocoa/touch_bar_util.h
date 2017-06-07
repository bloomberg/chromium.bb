// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_TOUCH_BAR_UTIL_H
#define UI_BASE_COCOA_TOUCH_BAR_UTIL_H

#import <Cocoa/Cocoa.h>

#include "ui/base/ui_base_export.h"

namespace ui {

// Returns the NSTouchBar Class.
UI_BASE_EXPORT Class NSTouchBar();

// Returns the NSCustomTouchBarItem Class.
UI_BASE_EXPORT Class NSCustomTouchBarItem();

// Returns a stylized blue button for the touch bar. The button performs
// |action| from the |target|.
UI_BASE_EXPORT NSButton* GetBlueTouchBarButton(NSString* title,
                                               id target,
                                               SEL action);

// Creates a touch bar identifier with the given |id|.
UI_BASE_EXPORT NSString* GetTouchBarId(NSString* touch_bar_id);

// Creates a touch Bar jtem identifier.
UI_BASE_EXPORT NSString* GetTouchBarItemId(NSString* touch_bar_id,
                                           NSString* item_id);

}  // namespace ui

#endif  // UI_BASE_COCOA_TOUCH_BAR_UTIL_H
