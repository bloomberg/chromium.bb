// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_EVENTS_SWITCH_ACCESS_EVENT_HANDLER_H_
#define ASH_EVENTS_SWITCH_ACCESS_EVENT_HANDLER_H_

#include "ash/ash_export.h"
#include "ash/public/interfaces/accessibility_controller.mojom.h"
#include "base/macros.h"
#include "ui/events/event_handler.h"

namespace ash {

// SwitchAccessEventHandler sends events to the Switch Access extension
// (via the delegate) when it is enabled.
class ASH_EXPORT SwitchAccessEventHandler : public ui::EventHandler {
 public:
  explicit SwitchAccessEventHandler(
      mojom::SwitchAccessEventHandlerDelegatePtr delegate_ptr);
  ~SwitchAccessEventHandler() override;

  // Sets the keys that are captured by Switch Access.
  void set_keys_to_capture(std::vector<int> keys) {
    keys_to_capture_ = std::set<int>(keys.begin(), keys.end());
  }

  // Sets whether virtual key events should be ignored.
  void set_ignore_virtual_key_events(bool should_ignore) {
    ignore_virtual_key_events_ = should_ignore;
  }

  // Tells the handler whether to forward all incoming key events to the Switch
  // Access extension.
  void set_forward_key_events(bool should_forward) {
    forward_key_events_ = should_forward;
  }

  // For testing usage only.
  void FlushMojoForTest();

 private:
  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;

  bool ShouldForwardEvent(const ui::KeyEvent& event) const;

  // The delegate used to send key events to the Switch Access extension.
  mojom::SwitchAccessEventHandlerDelegatePtr delegate_ptr_;

  std::set<int> keys_to_capture_;
  bool forward_key_events_ = false;
  bool ignore_virtual_key_events_ = true;

  DISALLOW_COPY_AND_ASSIGN(SwitchAccessEventHandler);
};

}  // namespace ash

#endif  // ASH_EVENTS_SWITCH_ACCESS_EVENT_HANDLER_H_
