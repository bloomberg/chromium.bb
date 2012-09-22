// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/mock_tsf_bridge.h"

#include "ui/base/ime/text_input_client.h"
#include "base/logging.h"

namespace ui {

MockTsfBridge::MockTsfBridge()
  : shutdown_call_count_(0),
    enable_ime_call_count_(0),
    disalbe_ime_call_count_(0),
    cancel_composition_call_count_(0),
    associate_focus_call_count_(0),
    set_focused_client_call_count_(0),
    remove_focused_client_call_count_(0),
    text_input_client_(NULL),
    focused_window_(NULL),
    latest_text_input_type_(TEXT_INPUT_TYPE_NONE) {
}

MockTsfBridge::~MockTsfBridge() {
}

void MockTsfBridge::Shutdown() {
  shutdown_call_count_++;
}

bool MockTsfBridge::CancelComposition() {
  ++cancel_composition_call_count_;
  return true;
}

void MockTsfBridge::OnTextInputTypeChanged(TextInputClient* client) {
  latest_text_input_type_ = client->GetTextInputType();
}

void MockTsfBridge::SetFocusedClient(HWND focused_window,
                                     TextInputClient* client) {
  ++set_focused_client_call_count_;
  focused_window_ = focused_window;
  text_input_client_ = client;
}

void MockTsfBridge::RemoveFocusedClient(TextInputClient* client) {
  ++remove_focused_client_call_count_;
  DCHECK_EQ(client, text_input_client_);
  text_input_client_ = NULL;
  focused_window_ = NULL;
}

void MockTsfBridge::Reset() {
  shutdown_call_count_ = 0;
  enable_ime_call_count_ = 0;
  disalbe_ime_call_count_ = 0;
  cancel_composition_call_count_ = 0;
  associate_focus_call_count_ = 0;
  set_focused_client_call_count_ = 0;
  remove_focused_client_call_count_ = 0;
  text_input_client_ = NULL;
  focused_window_ = NULL;
  latest_text_input_type_ = TEXT_INPUT_TYPE_NONE;
}

}  // namespace ui
