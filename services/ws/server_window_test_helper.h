// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_SERVER_WINDOW_TEST_HELPER_H_
#define SERVICES_WS_SERVER_WINDOW_TEST_HELPER_H_

#include "base/macros.h"
#include "ui/events/event.h"

namespace ws {

class ServerWindow;

// Used for accessing private members of ServerWindow in tests.
class ServerWindowTestHelper {
 public:
  explicit ServerWindowTestHelper(ServerWindow* server_window);
  ~ServerWindowTestHelper();

  bool IsHandlingPointerPress(ui::PointerId pointer_id);

 private:
  ServerWindow* server_window_;

  DISALLOW_COPY_AND_ASSIGN(ServerWindowTestHelper);
};

}  // namespace ws

#endif  // SERVICES_WS_SERVER_WINDOW_TEST_HELPER_H_
