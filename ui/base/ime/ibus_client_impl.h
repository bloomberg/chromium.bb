// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_IBUS_CLIENT_IMPL_H_
#define UI_BASE_IME_IBUS_CLIENT_IMPL_H_
#pragma once

#include "base/compiler_specific.h"
#include "ui/base/ime/ibus_client.h"

namespace ui {

namespace internal {

// An interface implemented by the object that sends and receives an event to
// and from ibus-daemon.
class UI_EXPORT IBusClientImpl : public IBusClient {
 public:
  IBusClientImpl();
  virtual ~IBusClientImpl();

  // ui::internal::IBusClient overrides:
  virtual bool IsConnected() OVERRIDE;
  virtual bool IsContextReady() OVERRIDE;
  virtual void CreateContext(PendingCreateICRequest* request) OVERRIDE;
  virtual void DestroyProxy() OVERRIDE;
  virtual void SetCapabilities(
      InlineCompositionCapability inline_type) OVERRIDE;
  virtual void FocusIn() OVERRIDE;
  virtual void FocusOut() OVERRIDE;
  virtual void Reset() OVERRIDE;
  virtual InputMethodType GetInputMethodType() OVERRIDE;
  virtual void SetCursorLocation(const gfx::Rect& cursor_location,
                                 const gfx::Rect& composition_head) OVERRIDE;
  virtual void SendKeyEvent(
      uint32 keyval,
      uint32 keycode,
      uint32 state,
      const chromeos::IBusInputContextClient::ProcessKeyEventCallback&
          cb) OVERRIDE;
 private:
  DISALLOW_COPY_AND_ASSIGN(IBusClientImpl);
};

}  // namespace internal
}  // namespace ui

#endif  // UI_BASE_IME_IBUS_CLIENT_IMPL_H_
