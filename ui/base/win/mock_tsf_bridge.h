// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_MOCK_TSF_BRIDGE_H_
#define UI_BASE_WIN_MOCK_TSF_BRIDGE_H_

#include "base/compiler_specific.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/win/tsf_bridge.h"

namespace ui {

class MockTsfBridge : public TsfBridge {
 public:
  MockTsfBridge();
  virtual ~MockTsfBridge();

  // TsfBridge override.
  virtual void Shutdown() OVERRIDE;

  // TsfBridge override.
  virtual bool CancelComposition() OVERRIDE;

  // TsfBridge override.
  virtual void OnTextInputTypeChanged(TextInputClient* client) OVERRIDE;

  // TsfBridge override.
  virtual void SetFocusedClient(HWND focused_window,
                                TextInputClient* client) OVERRIDE;

  // TsfBridge override.
  virtual void RemoveFocusedClient(TextInputClient* client) OVERRIDE;

  // Resets MockTsfBridge state including function call counter.
  void Reset();

  // Call count of Shutdown().
  int shutdown_call_count() const { return shutdown_call_count_; }

  // Call count of EnableIME().
  int enable_ime_call_count() const { return enable_ime_call_count_; }

  // Call count of DisableIME().
  int disalbe_ime_call_count() const { return disalbe_ime_call_count_; }

  // Call count of CancelComposition().
  int cancel_composition_call_count() const {
    return cancel_composition_call_count_;
  }

  // Call count of AssociateFocus().
  int associate_focus_call_count() const { return associate_focus_call_count_; }

  // Call count of SetFocusClient().
  int set_focused_client_call_count() const {
    return set_focused_client_call_count_;
  }

  // Call count of Shutdown().
  int remove_focused_client_call_count() const {
    return remove_focused_client_call_count_;
  }

  // Returns current TextInputClient.
  TextInputClient* text_input_clinet() const { return text_input_client_; }

  // Returns currently focused window handle.
  HWND focused_window() const { return focused_window_; }

  // Returns latest text input type.
  TextInputType latest_text_iput_type() const {
    return latest_text_input_type_;
  }

 private:
  int shutdown_call_count_;
  int enable_ime_call_count_;
  int disalbe_ime_call_count_;
  int cancel_composition_call_count_;
  int associate_focus_call_count_;
  int set_focused_client_call_count_;
  int remove_focused_client_call_count_;
  TextInputClient* text_input_client_;
  HWND focused_window_;
  TextInputType latest_text_input_type_;

  DISALLOW_COPY_AND_ASSIGN(MockTsfBridge);
};

}  // namespace ui

#endif  // UI_BASE_WIN_MOCK_TSF_BRIDGE_H_
