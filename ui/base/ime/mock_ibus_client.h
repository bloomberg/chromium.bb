// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_MOCK_IBUS_CLIENT_H_
#define UI_BASE_IME_MOCK_IBUS_CLIENT_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/ibus_client.h"

namespace ui {
namespace internal {

// A dummy IBusClient implementation for testing which requires neither
// ibus-daemon nor ibus header files.
class UI_EXPORT MockIBusClient : public IBusClient {
public:
  MockIBusClient();
  virtual ~MockIBusClient();

  // ui::internal::IBusClient overrides:
  virtual IBusBus* GetConnection() OVERRIDE;
  virtual bool IsConnected(IBusBus* bus) OVERRIDE;
  virtual void CreateContext(IBusBus* bus,
                             PendingCreateICRequest* request) OVERRIDE;
  virtual void DestroyProxy(IBusInputContext* context) OVERRIDE;
  virtual void SetCapabilities(
      IBusInputContext* context,
      InlineCompositionCapability inline_type) OVERRIDE;
  virtual void FocusIn(IBusInputContext* context) OVERRIDE;
  virtual void FocusOut(IBusInputContext* context) OVERRIDE;
  virtual void Reset(IBusInputContext* context) OVERRIDE;
  virtual InputMethodType GetInputMethodType() OVERRIDE;
  virtual void SetCursorLocation(IBusInputContext* context,
                                 const gfx::Rect& cursor_location,
                                 const gfx::Rect& composition_head) OVERRIDE;
  virtual void SendKeyEvent(IBusInputContext* context,
                            uint32 keyval,
                            uint32 keycode,
                            uint32 state,
                            PendingKeyEvent* pending_key) OVERRIDE;
  virtual void ExtractCompositionText(
      IBusText* text,
      guint cursor_position,
      CompositionText* out_composition) OVERRIDE;
  virtual string16 ExtractCommitText(IBusText* text) OVERRIDE;

  // See comments in CreateContext().
  enum CreateContextResult {
    kCreateContextSuccess,
    kCreateContextFail,
    kCreateContextNoResponse,
    kCreateContextDelayed,
  };

  // Resets all member variables below to the initial state.
  void ResetFlags();

  // Controls the behavior of CreateContext().
  CreateContextResult create_context_result_;
  scoped_ptr<PendingCreateICRequest> create_ic_request_;

  // A value which IsConnected() will return.
  bool is_connected_;
  // A value which GetInputMethodType() will return.
  InputMethodType input_method_type_;
  // A text which ExtractCompositionText() will return.
  CompositionText composition_text_;
  // A text which ExtractCommitText() will return.
  string16 commit_text_;

  unsigned int create_context_call_count_;
  unsigned int destroy_proxy_call_count_;
  unsigned int set_capabilities_call_count_;
  unsigned int focus_in_call_count_;
  unsigned int focus_out_call_count_;
  unsigned int reset_call_count_;
  unsigned int set_cursor_location_call_count_;

private:
  DISALLOW_COPY_AND_ASSIGN(MockIBusClient);
};

}  // namespace internal
}  // namespace ui

#endif  // UI_BASE_IME_MOCK_IBUS_CLIENT_H_
