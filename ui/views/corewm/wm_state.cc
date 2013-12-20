// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/wm_state.h"

#include "ui/views/corewm/transient_window_stacking_client.h"

namespace views {
namespace corewm {

WMState::WMState()
    : window_stacking_client_(new TransientWindowStackingClient) {
  aura::client::SetWindowStackingClient(window_stacking_client_.get());
}

WMState::~WMState() {
  if (aura::client::GetWindowStackingClient() == window_stacking_client_.get())
    aura::client::SetWindowStackingClient(NULL);
}

}  // namespace corewm
}  // namespace views
