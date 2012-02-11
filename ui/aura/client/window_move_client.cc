// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/window_move_client.h"

#include "ui/aura/window.h"
#include "ui/aura/window_property.h"

DECLARE_WINDOW_PROPERTY_TYPE(aura::client::WindowMoveClient*)

namespace aura {
namespace client {
namespace {

// A property key to store a client that handles window moves.
const WindowProperty<WindowMoveClient*> kWindowMoveClientProp = {NULL};
const WindowProperty<WindowMoveClient*>* const
    kWindowMoveClientKey = &kWindowMoveClientProp;

}  // namespace

void SetWindowMoveClient(Window* window, WindowMoveClient* client) {
  window->SetProperty(kWindowMoveClientKey, client);
}

WindowMoveClient* GetWindowMoveClient(Window* window) {
  return window->GetProperty(kWindowMoveClientKey);
}

}  // namespace client
}  // namespace aura
