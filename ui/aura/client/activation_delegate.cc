// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/activation_delegate.h"

#include "ui/aura/window.h"
#include "ui/aura/window_property.h"

DECLARE_WINDOW_PROPERTY_TYPE(aura::client::ActivationDelegate*)

namespace aura {
namespace client {
namespace {

// A property key to store the activation delegate for a window.
const WindowProperty<ActivationDelegate*> kActivationDelegateProp = {NULL};
const WindowProperty<ActivationDelegate*>* const
    kActivationDelegateKey = &kActivationDelegateProp;

}  // namespace

void SetActivationDelegate(Window* window, ActivationDelegate* delegate) {
  window->SetProperty(kActivationDelegateKey, delegate);
}

ActivationDelegate* GetActivationDelegate(Window* window) {
  return window->GetProperty(kActivationDelegateKey);
}

}  // namespace client
}  // namespace aura
