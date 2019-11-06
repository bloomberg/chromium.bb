// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_INPUT_METHOD_DELEGATE_H_
#define UI_BASE_IME_INPUT_METHOD_DELEGATE_H_

#include "base/callback.h"
#include "base/component_export.h"

namespace ui {

class KeyEvent;

struct EventDispatchDetails;

namespace internal {

// An interface implemented by the object that handles events sent back from an
// ui::InputMethod implementation.
class COMPONENT_EXPORT(UI_BASE_IME) InputMethodDelegate {
 public:
  virtual ~InputMethodDelegate() {}

  using DispatchKeyEventPostIMECallback = base::OnceCallback<void(bool, bool)>;
  // Dispatch a key event already processed by the input method. Returns the
  // status of processing, as well as running the callback |callback| with the
  // result of processing. |callback| may be run asynchronously (if the
  // delegate does processing async). Subclasses can use
  // RunDispatchKeyEventPostIMECallback() to run the callback. |callback| is
  // supplied two booleans that correspond to event->handled() and
  // event->stopped_propagation().
  virtual EventDispatchDetails DispatchKeyEventPostIME(
      KeyEvent* key_event,
      DispatchKeyEventPostIMECallback callback) = 0;

 protected:
  static void RunDispatchKeyEventPostIMECallback(
      KeyEvent* key_event,
      DispatchKeyEventPostIMECallback callback);
};

}  // namespace internal
}  // namespace ui

#endif  // UI_BASE_IME_INPUT_METHOD_DELEGATE_H_
