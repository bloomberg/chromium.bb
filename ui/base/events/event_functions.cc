// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/events/event_functions.h"

#include "ui/base/events/event.h"
#include "ui/base/events/event_constants.h"

namespace ui {

bool EventCanceledDefaultHandling(const ui::Event& event) {
  return event.phase() == EP_POSTTARGET && event.result() != ER_UNHANDLED;
}

}  // namespace ui

