// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/activation_client.h"

#include "ui/aura/root_window.h"
#include "ui/aura/window_property.h"

DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(AURA_EXPORT, aura::Window*)
DECLARE_WINDOW_PROPERTY_TYPE(aura::client::ActivationClient*)

namespace aura {
namespace client {
namespace {

const WindowProperty<Window*> kRootWindowActiveWindowProp = {NULL};

// A property key to store a client that handles window activation.
const WindowProperty<ActivationClient*>
    kRootWindowActivationClientProp = {NULL};
const WindowProperty<ActivationClient*>* const
    kRootWindowActivationClientKey = &kRootWindowActivationClientProp;

}  // namespace

const WindowProperty<Window*>* const
    kRootWindowActiveWindowKey = &kRootWindowActiveWindowProp;

void SetActivationClient(ActivationClient* client) {
  RootWindow::GetInstance()->SetProperty(kRootWindowActivationClientKey,
                                         client);
}

ActivationClient* GetActivationClient() {
  return RootWindow::GetInstance()->GetProperty(kRootWindowActivationClientKey);
}

}  // namespace client
}  // namespace aura
