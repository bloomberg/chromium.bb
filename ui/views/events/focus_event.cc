// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/events/focus_event.h"

namespace ui {

////////////////////////////////////////////////////////////////////////////////
// FocusEvent, public:

FocusEvent::FocusEvent(Type type, Reason reason, TraversalDirection direction)
    : type_(type),
      reason_(reason),
      direction_(direction),
      Event(ET_FOCUS_CHANGE, 0) {
}

}  // namespace ui
