// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_TOUCH_SELECTION_SELECTION_EVENT_TYPE_
#define UI_TOUCH_SELECTION_SELECTION_EVENT_TYPE_

namespace ui {

// This file contains a list of events relating to selection and insertion, used
// for notifying Java when the renderer selection has changed.

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.ui.touch_selection
enum SelectionEventType {
  SELECTION_SHOWN,
  SELECTION_CLEARED,
  SELECTION_DRAG_STARTED,
  SELECTION_DRAG_STOPPED,
  INSERTION_SHOWN,
  INSERTION_MOVED,
  INSERTION_TAPPED,
  INSERTION_CLEARED,
  INSERTION_DRAG_STARTED,
  INSERTION_DRAG_STOPPED,
};

}  // namespace ui

#endif  // UI_TOUCH_SELECTION_SELECTION_EVENT_TYPE_
