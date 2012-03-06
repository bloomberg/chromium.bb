// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/visibility_client.h"

#include "ui/aura/root_window.h"
#include "ui/aura/window_property.h"

DECLARE_WINDOW_PROPERTY_TYPE(aura::client::VisibilityClient*)

namespace aura {
namespace client {

// A property key to store a client that handles window visibility changes.
DEFINE_LOCAL_WINDOW_PROPERTY_KEY(
    VisibilityClient*, kRootWindowVisibilityClientKey, NULL);

void SetVisibilityClient(RootWindow* root_window, VisibilityClient* client) {
  root_window->SetProperty(kRootWindowVisibilityClientKey, client);
}

VisibilityClient* GetVisibilityClient(RootWindow* root_window) {
  return root_window ?
      root_window->GetProperty(kRootWindowVisibilityClientKey) : NULL;
}

}  // namespace client
}  // namespace aura
