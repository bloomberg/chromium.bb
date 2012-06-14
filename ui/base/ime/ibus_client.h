// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_IBUS_CLIENT_H_
#define UI_BASE_IME_IBUS_CLIENT_H_
#pragma once

#include <glib/gtypes.h>

#include "base/basictypes.h"
#include "base/event_types.h"
#include "base/string16.h"
#include "ui/base/events.h"
#include "ui/base/ui_export.h"

typedef struct _IBusBus IBusBus;
typedef struct _IBusInputContext IBusInputContext;
typedef struct _IBusText IBusText;

namespace gfx {
class Rect;
}  // namespace gfx
namespace ui {

struct CompositionText;

namespace internal {

// An interface implemented by the object that sends and receives an event to
// and from ibus-daemon.
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
    virtual ~PendingCreateICRequest() {};
    // Stores the result input context to |input_method_|, or abandon it if
    // |input_method_| is NULL.
    virtual void StoreOrAbandonInputContext(IBusInputContext* ic) = 0;
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

  // Gets a D-Bus connection to ibus-daemon. An implementation should establish
  // a connection to the daemon when the method is called first. After that, the
  // implementation should return the same object as before.
  virtual IBusBus* GetConnection() = 0;

  // Returns true if |bus| is connected.
  virtual bool IsConnected(IBusBus* bus) = 0;

  // Creates a new input context asynchronously. An implementation has to call
  // PendingCreateICRequest::StoreOrAbandonInputContext() with the newly created
  // context when the asynchronous request succeeds.
  virtual void CreateContext(IBusBus* bus,
                             PendingCreateICRequest* request) = 0;

  // Destroys the proxy object for the |context|. An implementation must send
  // "destroy" signal to the context object.
  virtual void DestroyProxy(IBusInputContext* context) = 0;

  // Updates the set of capabilities of the |context|.
  virtual void SetCapabilities(IBusInputContext* context,
                               InlineCompositionCapability inline_type) = 0;

  // Focuses the |context| asynchronously.
  virtual void FocusIn(IBusInputContext* context) = 0;
  // Blurs the |context| asynchronously.
  virtual void FocusOut(IBusInputContext* context) = 0;
  // Resets the |context| asynchronously.
  virtual void Reset(IBusInputContext* context) = 0;

  // Returns the current input method type.
  virtual InputMethodType GetInputMethodType() = 0;

  // Resets the cursor location asynchronously.
  virtual void SetCursorLocation(IBusInputContext* context,
                                 const gfx::Rect& cursor_location,
                                 const gfx::Rect& composition_head) = 0;

  // Sends the key to ibus-daemon asynchronously.
  virtual void SendKeyEvent(IBusInputContext* context,
                            uint32 keyval,
                            uint32 keycode,
                            uint32 state,
                            PendingKeyEvent* pending_key) = 0;

  // Called by InputMethodIBus::OnUpdatePreeditText to convert |text| into a
  // CompositionText.
  virtual void ExtractCompositionText(IBusText* text,
                                      guint cursor_position,
                                      CompositionText* out_composition) = 0;

  // Called by InputMethodIBus::OnCommitText to convert |text| into a Unicode
  // string.
  virtual string16 ExtractCommitText(IBusText* text) = 0;
};

}  // namespace internal
}  // namespace ui

#endif  // UI_BASE_IME_IBUS_CLIENT_H_
