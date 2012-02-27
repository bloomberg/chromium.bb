// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/stacking_client.h"

#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window_property.h"

DECLARE_WINDOW_PROPERTY_TYPE(aura::client::StackingClient*)

namespace aura {
namespace client {

void SetStackingClient(StackingClient* stacking_client) {
  Env::GetInstance()->set_stacking_client(stacking_client);
}

// static
StackingClient* GetStackingClient() {
  return Env::GetInstance()->stacking_client();
}

}  // namespace client
}  // namespace aura
