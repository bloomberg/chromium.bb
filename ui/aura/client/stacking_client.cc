// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/stacking_client.h"

#include "ui/aura/root_window.h"
#include "ui/aura/window_property.h"

DECLARE_WINDOW_PROPERTY_TYPE(aura::client::StackingClient*)

namespace aura {
namespace client {
namespace {

// A property key to store a client that handles window parenting.
const WindowProperty<StackingClient*> kRootWindowStackingClientProp = {NULL};
const WindowProperty<StackingClient*>* const
    kRootWindowStackingClientKey = &kRootWindowStackingClientProp;

}  // namespace

void SetStackingClient(StackingClient* stacking_client) {
  RootWindow::GetInstance()->SetProperty(kRootWindowStackingClientKey,
                                         stacking_client);
}

// static
StackingClient* GetStackingClient() {
  return RootWindow::GetInstance()->GetProperty(kRootWindowStackingClientKey);
}

}  // namespace client
}  // namespace aura
