// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_COCOA_EVENT_UTILS_H_
#define UI_BASE_COCOA_COCOA_EVENT_UTILS_H_

#import <Cocoa/Cocoa.h>

#include "ui/base/ui_export.h"
#include "ui/base/window_open_disposition.h"

namespace ui {

// Retrieves a bitsum of ui::EventFlags represented by |event|,
UI_EXPORT int EventFlagsFromNSEvent(NSEvent* event);

// Retrieves a bitsum of ui::EventFlags represented by |event|,
// but instead use the modifier flags given by |modifiers|,
// which is the same format as |-NSEvent modifierFlags|. This allows
// substitution of the modifiers without having to create a new event from
// scratch.
UI_EXPORT int EventFlagsFromNSEventWithModifiers(NSEvent* event,
                                                 NSUInteger modifiers);

// Retrieves the WindowOpenDisposition used to open a link from a user gesture
// represented by |event|. For example, a Cmd+Click would mean open the
// associated link in a background tab.
UI_EXPORT WindowOpenDisposition WindowOpenDispositionFromNSEvent(
    NSEvent* event);

// Retrieves the WindowOpenDisposition used to open a link from a user gesture
// represented by |event|, but instead use the modifier flags given by |flags|,
// which is the same format as |-NSEvent modifierFlags|. This allows
// substitution of the modifiers without having to create a new event from
// scratch.
UI_EXPORT WindowOpenDisposition WindowOpenDispositionFromNSEventWithFlags(
    NSEvent* event, NSUInteger flags);

}  // namespace ui

#endif  // UI_BASE_COCOA_COCOA_EVENT_UTILS_H_
