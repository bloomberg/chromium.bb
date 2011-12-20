// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/activation_client.h"

#include "ui/aura/root_window.h"

namespace aura {
namespace client {

const char kRootWindowActivationClient[] = "RootWindowActivationClient";
const char kRootWindowActiveWindow[] = "RootWindowActiveWindow";

void SetActivationClient(ActivationClient* client) {
  RootWindow::GetInstance()->SetProperty(kRootWindowActivationClient, client);
}

ActivationClient* GetActivationClient() {
  return reinterpret_cast<ActivationClient*>(
      RootWindow::GetInstance()->GetProperty(kRootWindowActivationClient));
}

}  // namespace client
}  // namespace aura
