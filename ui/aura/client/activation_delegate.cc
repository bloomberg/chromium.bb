// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/activation_delegate.h"

#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"

namespace aura {

// static
void ActivationDelegate::SetActivationDelegate(Window* window,
                                               ActivationDelegate* delegate) {
  window->SetProperty(kActivationDelegateKey, delegate);
}

// static
ActivationDelegate* ActivationDelegate::GetActivationDelegate(Window* window) {
  return reinterpret_cast<ActivationDelegate*>(
      window->GetProperty(kActivationDelegateKey));
}

}  // namespace aura
