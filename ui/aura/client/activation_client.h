// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_ACTIVATION_CLIENT_H_
#define UI_AURA_CLIENT_ACTIVATION_CLIENT_H_
#pragma once

#include "ui/aura/aura_export.h"

namespace aura {

class Window;

// An interface implemented by an object that manages window activation.
class AURA_EXPORT ActivationClient {
 public:
  // Sets/Gets the activation client on the RootWindow.
  static void SetActivationClient(ActivationClient* client);
  static ActivationClient* GetActivationClient();

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

}  // namespace aura

#endif  // UI_AURA_CLIENT_ACTIVATION_CLIENT_H_
