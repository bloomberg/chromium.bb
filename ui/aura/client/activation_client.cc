// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/activation_client.h"

#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"

namespace aura {

// static
void ActivationClient::SetActivationClient(ActivationClient* client) {
  RootWindow::GetInstance()->SetProperty(kRootWindowActivationClient, client);
}

// static
ActivationClient* ActivationClient::GetActivationClient() {
  return reinterpret_cast<ActivationClient*>(
      RootWindow::GetInstance()->GetProperty(kRootWindowActivationClient));
}

}  // namespace aura
