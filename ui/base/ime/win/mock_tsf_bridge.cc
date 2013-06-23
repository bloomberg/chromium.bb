// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/win/mock_tsf_bridge.h"

#include "ui/base/ime/text_input_client.h"
#include "base/logging.h"

namespace ui {

MockTSFBridge::MockTSFBridge()
  : enable_ime_call_count_(0),
    disalbe_ime_call_count_(0),
    cancel_composition_call_count_(0),
    confirm_composition_call_count_(0),
    on_text_layout_changed_(0),
    associate_focus_call_count_(0),
    set_focused_client_call_count_(0),
    remove_focused_client_call_count_(0),
    text_input_client_(NULL),
    focused_window_(NULL),
    latest_text_input_type_(TEXT_INPUT_TYPE_NONE) {
}

MockTSFBridge::~MockTSFBridge() {
}

bool MockTSFBridge::CancelComposition() {
  ++cancel_composition_call_count_;
  return true;
}

bool MockTSFBridge::ConfirmComposition() {
  ++confirm_composition_call_count_;
  return true;
}

void MockTSFBridge::OnTextInputTypeChanged(const TextInputClient* client) {
  latest_text_input_type_ = client->GetTextInputType();
}

void MockTSFBridge::OnTextLayoutChanged() {
  ++on_text_layout_changed_;
}

void MockTSFBridge::SetFocusedClient(HWND focused_window,
                                     TextInputClient* client) {
  ++set_focused_client_call_count_;
  focused_window_ = focused_window;
  text_input_client_ = client;
}

void MockTSFBridge::RemoveFocusedClient(TextInputClient* client) {
  ++remove_focused_client_call_count_;
  DCHECK_EQ(client, text_input_client_);
  text_input_client_ = NULL;
  focused_window_ = NULL;
}

base::win::ScopedComPtr<ITfThreadMgr> MockTSFBridge::GetThreadManager() {
  return thread_manager_;
}

TextInputClient* MockTSFBridge::GetFocusedTextInputClient() const {
  return text_input_client_;
}

void MockTSFBridge::Reset() {
  enable_ime_call_count_ = 0;
  disalbe_ime_call_count_ = 0;
  cancel_composition_call_count_ = 0;
  confirm_composition_call_count_ = 0;
  on_text_layout_changed_ = 0;
  associate_focus_call_count_ = 0;
  set_focused_client_call_count_ = 0;
  remove_focused_client_call_count_ = 0;
  text_input_client_ = NULL;
  focused_window_ = NULL;
  latest_text_input_type_ = TEXT_INPUT_TYPE_NONE;
}

}  // namespace ui
