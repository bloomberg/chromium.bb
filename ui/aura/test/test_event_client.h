// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_TEST_EVENT_CLIENT_H_
#define UI_AURA_TEST_TEST_EVENT_CLIENT_H_
#pragma once

#include "base/logging.h"
#include "ui/aura/client/event_client.h"

namespace aura {
namespace test {

class TestEventClient : public client::EventClient {
 public:
  explicit TestEventClient(RootWindow* root_window);
  virtual ~TestEventClient();

 private:
  // Overridden from client::EventClient:
  virtual bool CanProcessEventsWithinSubtree(
      const Window* window) const OVERRIDE;

  RootWindow* root_window_;
  client::EventClient* last_event_client_;

  DISALLOW_COPY_AND_ASSIGN(TestEventClient);
};

}  // namespace test
}  // namespace aura


#endif  // UI_AURA_TEST_TEST_EVENT_CLIENT_H_