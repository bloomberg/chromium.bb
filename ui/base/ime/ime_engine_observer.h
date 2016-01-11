// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_IME_ENGINE_OBSERVER_H_
#define UI_BASE_IME_IME_ENGINE_OBSERVER_H_

#include "build/build_config.h"
#include "ui/base/ime/ime_engine_handler_interface.h"

namespace ui {

class IMEEngineObserver {
 public:
  virtual ~IMEEngineObserver() {}

  // Called when the IME becomes the active IME.
  virtual void OnActivate(const std::string& engine_id) = 0;

  // Called when a text field gains focus, and will be sending key events.
  virtual void OnFocus(
      const IMEEngineHandlerInterface::InputContext& context) = 0;

  // Called when a text field loses focus, and will no longer generate events.
  virtual void OnBlur(int context_id) = 0;

  // Called when the user pressed a key with a text field focused.
  virtual void OnKeyEvent(
      const std::string& engine_id,
      const IMEEngineHandlerInterface::KeyboardEvent& event,
      IMEEngineHandlerInterface::KeyEventDoneCallback& key_data) = 0;

  // Called when Chrome terminates on-going text input session.
  virtual void OnReset(const std::string& engine_id) = 0;

  // Called when the IME is no longer active.
  virtual void OnDeactivated(const std::string& engine_id) = 0;

  // Called when composition bounds are changed.
  virtual void OnCompositionBoundsChanged(
      const std::vector<gfx::Rect>& bounds) = 0;

  // Returns whether the observer is interested in key events.
  virtual bool IsInterestedInKeyEvent() const = 0;

  // Called when a surrounding text is changed.
  virtual void OnSurroundingTextChanged(const std::string& engine_id,
                                        const std::string& text,
                                        int cursor_pos,
                                        int anchor_pos,
                                        int offset_pos) = 0;

// ChromeOS only APIs.
#if defined(OS_CHROMEOS)

  enum MouseButtonEvent {
    MOUSE_BUTTON_LEFT,
    MOUSE_BUTTON_RIGHT,
    MOUSE_BUTTON_MIDDLE,
  };

  // Called when an InputContext's properties change while it is focused.
  virtual void OnInputContextUpdate(
      const IMEEngineHandlerInterface::InputContext& context) = 0;



  // Called when the user clicks on an item in the candidate list.
  virtual void OnCandidateClicked(const std::string& engine_id,
                                  int candidate_id,
                                  MouseButtonEvent button) = 0;

  // Called when a menu item for this IME is interacted with.
  virtual void OnMenuItemActivated(const std::string& engine_id,
                                   const std::string& menu_id) = 0;
#endif
};

}  // namespace ui

#endif  // UI_BASE_IME_IME_ENGINE_OBSERVER_H_
