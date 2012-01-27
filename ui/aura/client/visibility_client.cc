// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/visibility_client.h"

#include "ui/aura/root_window.h"

namespace aura {
namespace client {
namespace {

// A property key to store a client that handles window visibility changes. The
// type of the value is |aura::client::VisibilityClient*|.
const char kRootWindowVisibilityClient[] = "RootWindowVisibilityClient";

}  // namespace

void SetVisibilityClient(VisibilityClient* client) {
  RootWindow::GetInstance()->SetProperty(kRootWindowVisibilityClient, client);
}

VisibilityClient* GetVisibilityClient() {
  return reinterpret_cast<VisibilityClient*>(
      RootWindow::GetInstance()->GetProperty(kRootWindowVisibilityClient));
}

}  // namespace client
}  // namespace aura
