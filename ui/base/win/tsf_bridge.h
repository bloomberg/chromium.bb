// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_TSF_BRIDGE_H_
#define UI_BASE_WIN_TSF_BRIDGE_H_

#include <Windows.h>
#include "base/basictypes.h"
#include "ui/base/ui_export.h"

namespace ui {
class TextInputClient;

// TsfBridge provides high level IME related operations on top of Text Services
// Framework (TSF). TsfBridge is managed by TLS because TSF related stuff is
// associated with each thread and not allowed to access across thread boundary.
// To be consistent with IMM32 behavior, TsfBridge is shared in the same thread.
// TsfBridge is used by the web content text inputting field, for example
// DisableIME() should be called if a password field is focused.
//
// TsfBridge also manages connectivity between TsfTextStore which is the backend
// of text inputting and current focused TextInputClient.
//
// All methods in this class must be used in UI thread.
class TsfBridge {
 public:
  virtual ~TsfBridge();

  // Returns the thread local TsfBridge instance. Initialize() must be called
  // first. Do not cache this pointer and use it after TsfBridge Shutdown().
  static UI_EXPORT TsfBridge* GetInstance();

  // Sets the thread local instance. Must be called before any calls to
  // GetInstance().
  static UI_EXPORT bool Initialize();

  // Destroys the thread local instance.
  virtual void Shutdown() = 0;

  // Handles TextInputTypeChanged event. RWHVW is responsible for calling this
  // handler whenever renderer's input text type is changed. Does nothing
  // unless |client| is focused.
  virtual void OnTextInputTypeChanged(TextInputClient* client) = 0;

  // Cancels the ongoing composition if exists.
  // Returns false if an error occures.
  virtual bool CancelComposition() = 0;

  // Sets currently focused TextInputClient.
  // Caller must free |client|.
  virtual void SetFocusedClient(HWND focused_window,
                                TextInputClient* client) = 0;

  // Removes currently focused TextInputClient.
  // Caller must free |client|.
  virtual void RemoveFocusedClient(TextInputClient* client) = 0;

 protected:
  // Uses GetInstance() instead.
  TsfBridge();

 private:
  DISALLOW_COPY_AND_ASSIGN(TsfBridge);
};

}  // namespace ui

#endif  // UI_BASE_WIN_TSF_BRIDGE_H_
