// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/dispatcher_client.h"

#include "ui/aura/root_window.h"

namespace aura {

namespace client {

namespace {

// A property key to store the nested dispatcher controller. The type of the
// value is |aura::client::DispatcherClient*|.
const char kDispatcherClient[] = "AuraDispatcherClient";

}  // namespace

void SetDispatcherClient(DispatcherClient* client) {
  RootWindow::GetInstance()->SetProperty(kDispatcherClient, client);
}

DispatcherClient* GetDispatcherClient() {
  return reinterpret_cast<DispatcherClient*>(
      RootWindow::GetInstance()->GetProperty(kDispatcherClient));
}

}  // namespace client
}  // namespace aura
