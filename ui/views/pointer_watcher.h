// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_POINTER_WATCHER_H_
#define UI_VIEWS_POINTER_WATCHER_H_

#include "ui/views/views_export.h"

namespace ui {
class MouseEvent;
class TouchEvent;
}

namespace views {

// An interface for read-only observation of pointer events (in particular, the
// events cannot be marked as handled). Only certain event types are supported.
// NOTE: The event.target is always null, because on mus the target window may
// be owned by another process.
class VIEWS_EXPORT PointerWatcher {
 public:
  virtual ~PointerWatcher() {}

  virtual void OnMousePressed(const ui::MouseEvent& event) = 0;
  virtual void OnTouchPressed(const ui::TouchEvent& event) = 0;
};

}  // namespace views

#endif  // UI_VIEWS_POINTER_WATCHER_H_
