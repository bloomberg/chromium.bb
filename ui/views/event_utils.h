// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EVENT_UTILS_H_
#define UI_VIEWS_EVENT_UTILS_H_

#include "ui/gfx/native_widget_types.h"
#include "ui/views/views_export.h"

namespace ui {
class LocatedEvent;
}

namespace views {

// Reposts a located event natively. Returns false when |event| could not be
// reposted. |event| should be in screen coordinates. |window| is the target
// window that the event will be forwarded to. Only some events are supported.
VIEWS_EXPORT bool RepostLocatedEvent(gfx::NativeWindow window,
                                     const ui::LocatedEvent& event);

}  // namespace views

#endif  // UI_VIEWS_EVENT_UTILS_H_
