// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_ACTIVATION_CLIENT_H_
#define UI_AURA_CLIENT_ACTIVATION_CLIENT_H_
#pragma once

#include "ui/aura/aura_export.h"

namespace aura {
class Window;
namespace client {

// A property key to store a client that handles window activation. The type of
// the value is |aura::client::ActivationClient*|.
AURA_EXPORT extern const char kRootWindowActivationClient[];

// A property key to store what the client defines as the active window on the
// RootWindow. The type of the value is |aura::Window*|.
AURA_EXPORT extern const char kRootWindowActiveWindow[];

// An interface implemented by an object that manages window activation.
class AURA_EXPORT ActivationClient {
 public:
  // Activates |window|. If |window| is NULL, nothing happens.
  virtual void ActivateWindow(Window* window) = 0;

  // Deactivates |window|. What (if anything) is activated next is up to the
  // client. If |window| is NULL, nothing happens.
  virtual void DeactivateWindow(Window* window) = 0;

  // Retrieves the active window, or NULL if there is none.
  virtual aura::Window* GetActiveWindow() = 0;

  // Returns true if |window| can be focused. To be focusable, |window| must
  // exist inside an activatable window.
  virtual bool CanFocusWindow(Window* window) const = 0;

 protected:
  virtual ~ActivationClient() {}
};

// Sets/Gets the activation client on the RootWindow.
AURA_EXPORT void SetActivationClient(ActivationClient* client);
AURA_EXPORT ActivationClient* GetActivationClient();

}  // namespace clients
}  // namespace aura

#endif  // UI_AURA_CLIENT_ACTIVATION_CLIENT_H_
