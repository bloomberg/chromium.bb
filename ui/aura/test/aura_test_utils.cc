// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/aura_test_utils.h"

#include "ui/aura/window_tree_host.h"

namespace aura {
namespace test {

class WindowTreeHostTestApi {
 public:
  explicit WindowTreeHostTestApi(WindowTreeHost* host) : host_(host) {}

  const gfx::Point& last_cursor_request_position_in_host() {
    return host_->last_cursor_request_position_in_host_;
  }

 private:
  WindowTreeHost* host_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostTestApi);
};

const gfx::Point& QueryLatestMousePositionRequestInHost(WindowTreeHost* host) {
  WindowTreeHostTestApi host_test_api(host);
  return host_test_api.last_cursor_request_position_in_host();
}

}  // namespace test
}  // namespace aura
