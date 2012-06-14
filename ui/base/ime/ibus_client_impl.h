// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_IBUS_CLIENT_IMPL_H_
#define UI_BASE_IME_IBUS_CLIENT_IMPL_H_
#pragma once

#include "base/compiler_specific.h"
#include "ui/base/ime/ibus_client.h"

namespace ui {

static const int kIBusReleaseMask = 1 << 30;

namespace internal {

// An interface implemented by the object that sends and receives an event to
// and from ibus-daemon.
class UI_EXPORT IBusClientImpl : public IBusClient {
public:
  IBusClientImpl();
  virtual ~IBusClientImpl();

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

private:
  DISALLOW_COPY_AND_ASSIGN(IBusClientImpl);
};

}  // namespace internal
}  // namespace ui

#endif  // UI_BASE_IME_IBUS_CLIENT_IMPL_H_
