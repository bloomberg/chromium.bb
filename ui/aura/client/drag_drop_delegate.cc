// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/drag_drop_delegate.h"

#include "ui/aura/window.h"

namespace aura {
namespace client {

const char kDragDropDelegateKey[] = "DragDropDelegate";

void SetDragDropDelegate(Window* window, DragDropDelegate* delegate) {
  window->SetProperty(kDragDropDelegateKey, delegate);
}

DragDropDelegate* GetDragDropDelegate(Window* window) {
  return reinterpret_cast<DragDropDelegate*>(
      window->GetProperty(kDragDropDelegateKey));
}

}  // namespace client
}  // namespace aura