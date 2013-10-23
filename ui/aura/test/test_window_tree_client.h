// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_TEST_WINDOW_TREE_CLIENT_H_
#define UI_AURA_TEST_TEST_WINDOW_TREE_CLIENT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/client/window_tree_client.h"

namespace aura {
namespace test {

class TestWindowTreeClient : public client::WindowTreeClient {
 public:
  explicit TestWindowTreeClient(Window* root_window);
  virtual ~TestWindowTreeClient();

  // Overridden from client::WindowTreeClient:
  virtual Window* GetDefaultParent(Window* context,
                                   Window* window,
                                   const gfx::Rect& bounds) OVERRIDE;
 private:
  Window* root_window_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowTreeClient);
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_TEST_WINDOW_TREE_CLIENT_H_
