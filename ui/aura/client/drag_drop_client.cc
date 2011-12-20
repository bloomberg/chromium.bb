// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/drag_drop_client.h"

#include "ui/aura/root_window.h"

namespace aura {
namespace client {

const char kRootWindowDragDropClientKey[] = "RootWindowDragDropClient";

void SetDragDropClient(DragDropClient* client) {
  RootWindow::GetInstance()->SetProperty(kRootWindowDragDropClientKey, client);
}

DragDropClient* GetDragDropClient() {
  return reinterpret_cast<DragDropClient*>(
      RootWindow::GetInstance()->GetProperty(kRootWindowDragDropClientKey));
}

}  // namespace client
}  // namespace aura
