// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_EVENTS_EVENT_FUNCTIONS_H_
#define UI_BASE_EVENTS_EVENT_FUNCTIONS_H_

#include "ui/base/ui_export.h"

namespace ui {

class Event;

// Returns true if default post-target handling was canceled for |event| after
// its dispatch to its target.
UI_EXPORT bool EventCanceledDefaultHandling(const ui::Event& event);

}  // namespace ui

#endif  // UI_BASE_EVENTS_EVENT_FUNCTIONS_H_
