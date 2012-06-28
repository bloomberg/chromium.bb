// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// TODO(nona): Remove this file.

#include "ui/base/ime/mock_ibus_client.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"

namespace ui {
namespace internal {

MockIBusClient::MockIBusClient() {
  ResetFlags();
}

MockIBusClient::~MockIBusClient() {
  if (create_ic_request_.get() &&
      (create_context_result_ == kCreateContextDelayed)) {
    // The destructor is called after ui::InputMethodIBus is destructed. Check
    // whether the new context created by CreateContextObject() is immediately
    // destroyed by checking the counter in MockIBusClient::DestroyProxy().
    const unsigned int initial_call_count = destroy_proxy_call_count_;
    create_ic_request_->InitOrAbandonInputContext();
    DCHECK_EQ(initial_call_count + 1, destroy_proxy_call_count_);
  }
}

bool MockIBusClient::IsConnected() {
  return is_connected_;
}

bool MockIBusClient::IsContextReady() {
  return is_context_ready_;
}

void MockIBusClient::CreateContext(PendingCreateICRequest* request) {
  ++create_context_call_count_;
  switch (create_context_result_) {
    case kCreateContextSuccess:
      // Create a new context immediately.
      is_context_ready_ = true;
      request->InitOrAbandonInputContext();
      delete request;
      break;
    case kCreateContextFail:
      // Emulate an IPC failure. Pass NULL to the request object.
      request->OnCreateInputContextFailed();
      delete request;
      break;
    case kCreateContextNoResponse:
      // Emulate ibus-daemon hang-up. Do not call StoreOrAbandonInputContext.
      create_ic_request_.reset(request);
      break;
    case kCreateContextDelayed:
      // Emulate overloaded ibus-daemon. Call StoreOrAbandonInputContext later.
      create_ic_request_.reset(request);
      break;
  }
}

void MockIBusClient::DestroyProxy() {
  ++destroy_proxy_call_count_;
  is_context_ready_ = false;
}

void MockIBusClient::SetCapabilities(InlineCompositionCapability inline_type) {
  ++set_capabilities_call_count_;
}

void MockIBusClient::FocusIn() {
  ++focus_in_call_count_;
}

void MockIBusClient::FocusOut() {
  ++focus_out_call_count_;
}

void MockIBusClient::Reset() {
  ++reset_call_count_;
}

IBusClient::InputMethodType MockIBusClient::GetInputMethodType() {
  return input_method_type_;
}

void MockIBusClient::SetCursorLocation(const gfx::Rect& cursor_location,
                                       const gfx::Rect& composition_head) {
  ++set_cursor_location_call_count_;
}

void MockIBusClient::SendKeyEvent(
    uint32 keyval,
    uint32 keycode,
    uint32 state,
    const chromeos::IBusInputContextClient::ProcessKeyEventCallback& callback) {
  // TODO(yusukes): implement this function.
}

void MockIBusClient::ResetFlags() {
  create_context_result_ = kCreateContextSuccess;
  create_ic_request_.reset();

  is_connected_ = false;
  is_context_ready_ = false;
  input_method_type_ = INPUT_METHOD_NORMAL;
  commit_text_.clear();

  create_context_call_count_ = 0;
  destroy_proxy_call_count_ = 0;
  set_capabilities_call_count_ = 0;
  focus_in_call_count_ = 0;
  focus_out_call_count_ = 0;
  reset_call_count_ = 0;
  set_cursor_location_call_count_ = 0;
}

}  // namespace internal
}  // namespace ui
