// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/window_move_client.h"

#include "ui/aura/window.h"

namespace aura {
namespace client {

const char kWindowMoveClientKey[] = "WindowMoveClient";

void SetWindowMoveClient(Window* window, WindowMoveClient* client) {
  window->SetProperty(kWindowMoveClientKey, client);
}

WindowMoveClient* GetWindowMoveClient(Window* window) {
  return reinterpret_cast<WindowMoveClient*>(
      window->GetProperty(kWindowMoveClientKey));
}

}  // namespace client
}  // namespace aura
