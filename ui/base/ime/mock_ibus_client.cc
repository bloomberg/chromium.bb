// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/mock_ibus_client.h"

#include <glib-object.h>

#include "base/logging.h"
#include "base/utf_string_conversions.h"

// Define a dummy IBusBus class. A real GObject is necessary here to make it
// possible to send/receive GObject signals through the object.
typedef struct _IBusBus {
  GObject parent;
} IBusBus;
typedef struct _IBusBusClass {
  GObjectClass parent;
} IBusBusClass;
G_DEFINE_TYPE(IBusBus, ibus_bus, G_TYPE_OBJECT)

// Define a dummy IBusText class.
typedef struct _IBusText {
  GObject parent;
} IBusText;
typedef struct _IBusTextClass {
  GObjectClass parent;
} IBusTextClass;
G_DEFINE_TYPE(IBusText, ibus_text, G_TYPE_OBJECT)

// Define a dummy IBusInputContext class.
typedef struct _IBusInputContext {
  GObject parent;
} IBusInputContext;
typedef struct _IBusInputContextClass {
  GObjectClass parent;
} IBusInputContextClass;
G_DEFINE_TYPE(IBusInputContext, ibus_input_context, G_TYPE_OBJECT)

namespace {

IBusBus* CreateBusObject() {
  return reinterpret_cast<IBusBus*>(g_object_new(ibus_bus_get_type(), NULL));
}

IBusInputContext* CreateContextObject() {
  return reinterpret_cast<IBusInputContext*>(
      g_object_new(ibus_input_context_get_type(), NULL));
}

void AddVoidSignal(gpointer klass, const char* signal_name) {
  g_signal_new(signal_name,
               G_TYPE_FROM_CLASS(klass),
               G_SIGNAL_RUN_LAST,
               0,  // class_offset
               NULL,  // accumulator
               NULL,  // accu_data
               g_cclosure_marshal_VOID__VOID,
               G_TYPE_NONE,
               0);
}

}  // namespace

static void ibus_bus_init(IBusBus* obj) {}
static void ibus_bus_class_init(IBusBusClass* klass) {
  AddVoidSignal(klass, "connected");
  AddVoidSignal(klass, "disconnected");
}
static void ibus_text_init(IBusText* obj) {}
static void ibus_text_class_init(IBusTextClass* klass) {}
static void ibus_input_context_init(IBusInputContext* obj) {}
static void ibus_input_context_class_init(IBusInputContextClass* klass) {
  AddVoidSignal(klass, "show-preedit-text");
  AddVoidSignal(klass, "hide-preedit-text");
  AddVoidSignal(klass, "destroy");

  g_signal_new("commit-text",
               G_TYPE_FROM_CLASS(klass),
               G_SIGNAL_RUN_LAST,
               0,
               NULL,
               NULL,
               g_cclosure_marshal_VOID__OBJECT,
               G_TYPE_NONE,
               1,
               ibus_text_get_type());

  // TODO(yusukes): Support forward-key-event and update-preedit-text signals.
  // To do that, we have to generate custom marshallers using glib-genmarshal.

  /*
  g_signal_new("forward-key-event",
               G_TYPE_FROM_CLASS(klass),
               G_SIGNAL_RUN_LAST,
               0,
               NULL,
               NULL,
               marshal_VOID__UINT_UNIT_UNIT,
               G_TYPE_NONE,
               3,
               G_TYPE_UINT,
               G_TYPE_UINT,
               G_TYPE_UINT);

  g_signal_new("update-preedit-text",
               G_TYPE_FROM_CLASS(klass),
               G_SIGNAL_RUN_LAST,
               0,
               NULL,
               NULL,
               marshal_VOID__OBJECT_UINT_BOOLEAN,
               G_TYPE_NONE,
               3,
               IBUS_TYPE_TEXT,
               G_TYPE_UINT,
               G_TYPE_BOOLEAN);
  */
}

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
    create_ic_request_->StoreOrAbandonInputContext(CreateContextObject());
    DCHECK_EQ(initial_call_count + 1, destroy_proxy_call_count_);
  }
}

IBusBus* MockIBusClient::GetConnection() {
  g_type_init();
  return CreateBusObject();
}

bool MockIBusClient::IsConnected(IBusBus* bus) {
  return is_connected_;
}

void MockIBusClient::CreateContext(IBusBus* bus,
                                   PendingCreateICRequest* request) {
  ++create_context_call_count_;

  switch (create_context_result_) {
    case kCreateContextSuccess:
      // Create a new context immediately.
      request->StoreOrAbandonInputContext(CreateContextObject());
      delete request;
      break;
    case kCreateContextFail:
      // Emulate an IPC failure. Pass NULL to the request object.
      request->StoreOrAbandonInputContext(NULL);
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

void MockIBusClient::DestroyProxy(IBusInputContext* context) {
  ++destroy_proxy_call_count_;
  g_signal_emit_by_name(context, "destroy");
}

void MockIBusClient::SetCapabilities(IBusInputContext* context,
                                     InlineCompositionCapability inline_type) {
  ++set_capabilities_call_count_;
}

void MockIBusClient::FocusIn(IBusInputContext* context) {
  ++focus_in_call_count_;
}

void MockIBusClient::FocusOut(IBusInputContext* context) {
  ++focus_out_call_count_;
}

void MockIBusClient::Reset(IBusInputContext* context) {
  ++reset_call_count_;
}

IBusClient::InputMethodType MockIBusClient::GetInputMethodType() {
  return input_method_type_;
}

void MockIBusClient::SetCursorLocation(IBusInputContext* context,
                                       const gfx::Rect& cursor_location,
                                       const gfx::Rect& composition_head) {
  ++set_cursor_location_call_count_;
}

void MockIBusClient::SendKeyEvent(IBusInputContext* context,
                                  uint32 keyval,
                                  uint32 keycode,
                                  uint32 state,
                                  PendingKeyEvent* pending_key) {
  // TODO(yusukes): implement this function.
}

void MockIBusClient::ExtractCompositionText(IBusText* text,
                                            guint cursor_position,
                                            CompositionText* out_composition) {
  *out_composition = composition_text_;
}

string16 MockIBusClient::ExtractCommitText(IBusText* text) {
  return commit_text_;
}

void MockIBusClient::ResetFlags() {
  create_context_result_ = kCreateContextSuccess;
  create_ic_request_.reset();

  is_connected_ = false;
  input_method_type_ = INPUT_METHOD_NORMAL;
  composition_text_.Clear();
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
