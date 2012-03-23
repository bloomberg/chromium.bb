// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/test_event_client.h"

namespace aura {
namespace test {

TestEventClient::TestEventClient(RootWindow* root_window)
    : root_window_(root_window),
      last_event_client_(NULL) {
  last_event_client_ = client::GetEventClient(root_window_);
  client::SetEventClient(root_window_, this);
}

TestEventClient::~TestEventClient() {
  client::SetEventClient(root_window_, last_event_client_);
}

bool TestEventClient::CanProcessEventsWithinSubtree(const Window* window) {
  // This can change to user-settable state should anyone ever need to test
  // different return values.
  return true;
}

}  // namespace test
}  // namespace aura
