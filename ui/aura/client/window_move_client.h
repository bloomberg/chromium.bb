// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_WINDOW_MOVE_CLIENT_H_
#define UI_AURA_CLIENT_WINDOW_MOVE_CLIENT_H_
#pragma once

#include "ui/aura/aura_export.h"

namespace aura {
class Window;
namespace client {

// An interface implemented by an object that manages programatically keyed
// window moving.
class AURA_EXPORT WindowMoveClient {
 public:
  // Starts a nested message loop for moving the window.
  virtual void RunMoveLoop(Window* window) = 0;

  // Ends a previously started move loop.
  virtual void EndMoveLoop() = 0;

 protected:
  virtual ~WindowMoveClient() {}
};

// Sets/Gets the activation client for the specified window.
AURA_EXPORT void SetWindowMoveClient(Window* window,
                                     WindowMoveClient* client);
AURA_EXPORT WindowMoveClient* GetWindowMoveClient(Window* window);

}  // namespace client
}  // namespace aura

#endif  // UI_AURA_CLIENT_WINDOW_MOVE_CLIENT_H_
