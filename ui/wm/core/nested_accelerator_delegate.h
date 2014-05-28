// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_NESTED_ACCELERATOR_DELEGATE_H_
#define UI_WM_CORE_NESTED_ACCELERATOR_DELEGATE_H_

namespace ui {
class KeyEvent;
}

namespace wm {

// A delegate interface that implements the behavior of nested accelerator
// handling.
class NestedAcceleratorDelegate {
 public:
  virtual ~NestedAcceleratorDelegate() {}

  // If the key event should be ignored now and instead be reposted so that next
  // event loop.
  virtual bool ShouldProcessEventNow(const ui::KeyEvent& key_event) = 0;

  // Attempts to process an accelerator for the key-event.
  // Returns whether an accelerator was triggered and processed.
  virtual bool ProcessEvent(const ui::KeyEvent& key_event) = 0;
};

}  // namespace wm

#endif  // UI_WM_CORE_NESTED_ACCELERATOR_DELEGATE_H_
