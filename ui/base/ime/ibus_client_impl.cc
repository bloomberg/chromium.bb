// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/ibus_client_impl.h"

#include "base/basictypes.h"
#include "base/i18n/char_iterator.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/ibus/ibus_client.h"
#include "chromeos/dbus/ibus/ibus_input_context_client.h"
#include "ui/base/ime/composition_text.h"
#include "ui/gfx/rect.h"

namespace ui {
namespace internal {

namespace {

const char kClientName[] = "chrome";

// Following capability mask is introduced from
// http://ibus.googlecode.com/svn/docs/ibus-1.4/ibus-ibustypes.html#IBusCapabilite
const uint32 kIBusCapabilityPreeditText = 1U;
const uint32 kIBusCapabilityFocus = 8U;

chromeos::IBusInputContextClient* GetInputContextClient() {
  chromeos::IBusInputContextClient* client =
      chromeos::DBusThreadManager::Get()->GetIBusInputContextClient();
  DCHECK(client->IsConnected());
  return client;
}

// TODO(nona): Move this function to InputMethodIBus
void CreateInputContextDone(IBusClientImpl::PendingCreateICRequest* ic_request,
                            const dbus::ObjectPath& object_path) {
  chromeos::DBusThreadManager::Get()->GetIBusInputContextClient()
      ->Initialize(chromeos::DBusThreadManager::Get()->GetIBusBus(),
                   object_path);

  ic_request->InitOrAbandonInputContext();
  delete ic_request;
}

// TODO(nona): Move this function to InputMethodIBus
void CreateInputContextFail(
    IBusClientImpl::PendingCreateICRequest* ic_request) {
  ic_request->OnCreateInputContextFailed();
  delete ic_request;
}

}  // namespace

IBusClientImpl::IBusClientImpl() {
}

IBusClientImpl::~IBusClientImpl() {
}

bool IBusClientImpl::IsConnected() {
  return chromeos::DBusThreadManager::Get()->GetIBusBus() != NULL;
}

bool IBusClientImpl::IsContextReady() {
  return IsConnected() &&
      chromeos::DBusThreadManager::Get()->GetIBusInputContextClient()
          ->IsConnected();
}

void IBusClientImpl::CreateContext(PendingCreateICRequest* request) {
  DCHECK(IsConnected());
  chromeos::DBusThreadManager::Get()->GetIBusClient()->CreateInputContext(
      kClientName,
      base::Bind(&CreateInputContextDone,
                 base::Unretained(request)),
      base::Bind(&CreateInputContextFail,
                 base::Unretained(request)));
}

void IBusClientImpl::DestroyProxy() {
  GetInputContextClient()->ResetObjectProxy();
}

void IBusClientImpl::SetCapabilities(InlineCompositionCapability inline_type) {
  // TODO(nona): support surrounding text.
  uint32 capabilities =
      inline_type == INLINE_COMPOSITION ?
          kIBusCapabilityPreeditText | kIBusCapabilityFocus
          : kIBusCapabilityFocus;
  GetInputContextClient()->SetCapabilities(capabilities);
}

void IBusClientImpl::FocusIn() {
  GetInputContextClient()->FocusIn();
}

void IBusClientImpl::FocusOut() {
  GetInputContextClient()->FocusOut();
}

void IBusClientImpl::Reset() {
  GetInputContextClient()->Reset();
}

IBusClient::InputMethodType IBusClientImpl::GetInputMethodType() {
  // This object cannot know the type of the current IME, hence return NORMAL.
  return INPUT_METHOD_NORMAL;
}

void IBusClientImpl::SetCursorLocation(const gfx::Rect& cursor_location,
                                       const gfx::Rect& composition_head) {
  GetInputContextClient()->SetCursorLocation(cursor_location.x(),
                                             cursor_location.y(),
                                             cursor_location.width(),
                                             cursor_location.height());
}

void IBusClientImpl::SendKeyEvent(
    uint32 keyval,
    uint32 keycode,
    uint32 state,
    const chromeos::IBusInputContextClient::ProcessKeyEventCallback& callback) {
  GetInputContextClient()->ProcessKeyEvent(keyval, keycode, state, callback);
}

}  // namespace internal
}  // namespace ui
