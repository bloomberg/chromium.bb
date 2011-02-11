// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EVENTS_EVENT_UTILS_WIN_H_
#define VIEWS_EVENTS_EVENT_UTILS_WIN_H_
#pragma once

#include "ui/gfx/native_widget_types.h"

// Windows-specific Event utilities. Add functionality here rather than adding
// #ifdefs to event.h

namespace views {

class Event;
class KeyEvent;

// Returns the repeat count of the specified KeyEvent. Valid only for
// KeyEvents constructed from a MSG.
int GetRepeatCount(const KeyEvent& event);

// Returns true if the affected key is a Windows extended key. See documentation
// for WM_KEYDOWN for explanation.
// Valid only for KeyEvents constructed from a MSG.
bool IsExtendedKey(const KeyEvent& event);

// Return a mask of windows key/button state flags for the event object.
int GetWindowsFlags(const Event& event);

}  // namespace views

#endif  // VIEWS_EVENTS_EVENT_UTILS_WIN_H_
