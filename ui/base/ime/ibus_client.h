// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_IBUS_CLIENT_H_
#define UI_BASE_IME_IBUS_CLIENT_H_
#pragma once

#include "base/basictypes.h"
#include "base/event_types.h"
#include "base/string16.h"
#include "chromeos/dbus/ibus/ibus_client.h"
#include "chromeos/dbus/ibus/ibus_input_context_client.h"
#include "chromeos/dbus/ibus/ibus_text.h"
#include "ui/base/events.h"
#include "ui/base/ui_export.h"
#include "ui/gfx/rect.h"

namespace gfx {
class Rect;
}  // namespace gfx
namespace ui {

struct CompositionText;

namespace internal {

// An interface implemented by the object that sends and receives an event to
// and from ibus-daemon.
// TODO(nona): Remove all methods except GetInputMethodType and
// SetCursorLocation.
class UI_EXPORT IBusClient {
 public:
  // A class to hold all data related to a key event being processed by the
  // input method but still has no result back yet.
  class PendingKeyEvent {
   public:
    virtual ~PendingKeyEvent() {}
    // Process this pending key event after we receive its result from the input
    // method. It just call through InputMethodIBus::ProcessKeyEventPostIME().
    virtual void ProcessPostIME(bool handled) = 0;
  };

  // A class to hold information of a pending request for creating an ibus input
  // context.
  class PendingCreateICRequest {
   public:
    virtual ~PendingCreateICRequest() {}
    // Set up signal handlers, or destroy object proxy if the input context is
    // already abandoned.
    virtual void InitOrAbandonInputContext() = 0;

    // Called if the create input context method call is failed.
    virtual void OnCreateInputContextFailed() = 0;
  };

  enum InlineCompositionCapability {
    OFF_THE_SPOT_COMPOSITION = 0,
    INLINE_COMPOSITION = 1,
  };

  // The type of IME which is currently selected. Implementations should return
  // the former when no IME is selected or the type of the current IME is
  // unknown.
  enum InputMethodType {
    INPUT_METHOD_NORMAL = 0,
    INPUT_METHOD_XKB_LAYOUT,
  };

  virtual ~IBusClient() {}

  // Returns true if the connection to ibus-daemon is established.
  virtual bool IsConnected() = 0;

  // Returns true if the input context is ready to use.
  virtual bool IsContextReady() = 0;

  // Creates a new input context asynchronously. An implementation has to call
  // PendingCreateICRequest::StoreOrAbandonInputContext() with the newly created
  // context when the asynchronous request succeeds.
  // TODO(nona): We can omit the first argument(need unittests fix).
  virtual void CreateContext(PendingCreateICRequest* request) = 0;

  // Destroys the proxy object in input context client.
  virtual void DestroyProxy() = 0;

  // Updates the set of capabilities.
  virtual void SetCapabilities(InlineCompositionCapability inline_type) = 0;

  // Focuses the context asynchronously.
  virtual void FocusIn() = 0;
  // Blurs the context asynchronously.
  virtual void FocusOut() = 0;
  // Resets the context asynchronously.
  virtual void Reset() = 0;

  // Returns the current input method type.
  virtual InputMethodType GetInputMethodType() = 0;

  // Resets the cursor location asynchronously.
  virtual void SetCursorLocation(const gfx::Rect& cursor_location,
                                 const gfx::Rect& composition_head) = 0;

  // Sends the key to ibus-daemon asynchronously.
  virtual void SendKeyEvent(
      uint32 keyval,
      uint32 keycode,
      uint32 state,
      const chromeos::IBusInputContextClient::ProcessKeyEventCallback& cb) = 0;
};

}  // namespace internal
}  // namespace ui

#endif  // UI_BASE_IME_IBUS_CLIENT_H_
