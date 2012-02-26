// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/drag_drop_client.h"

#include "ui/aura/root_window.h"
#include "ui/aura/window_property.h"

DECLARE_WINDOW_PROPERTY_TYPE(aura::client::DragDropClient*)

namespace aura {
namespace client {
namespace {

// A property key to store the drag and drop client for the root window.
const WindowProperty<DragDropClient*> kRootWindowDragDropClientProp = {NULL};
const WindowProperty<DragDropClient*>* const
    kRootWindowDragDropClientKey = &kRootWindowDragDropClientProp;

}  // namespace

void SetDragDropClient(RootWindow* root_window, DragDropClient* client) {
  root_window->SetProperty(kRootWindowDragDropClientKey, client);
}

DragDropClient* GetDragDropClient(RootWindow* root_window) {
  return root_window ?
      root_window->GetProperty(kRootWindowDragDropClientKey) : NULL;
}

}  // namespace client
}  // namespace aura
