// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/events/event_constants.h"

#include "base/logging.h"

namespace ui {

EventResult EventResultFromTouchStatus(TouchStatus status) {
  switch (status) {
    case TOUCH_STATUS_UNKNOWN:
      return ER_UNHANDLED;

    case TOUCH_STATUS_START:
    case TOUCH_STATUS_CONTINUE:
    case TOUCH_STATUS_END:
      return ER_CONSUMED;

    case TOUCH_STATUS_QUEUED:
    case TOUCH_STATUS_QUEUED_END:
      return ER_ASYNC;
  }

  NOTREACHED();
  return ER_UNHANDLED;
}

}  // namespace ui
