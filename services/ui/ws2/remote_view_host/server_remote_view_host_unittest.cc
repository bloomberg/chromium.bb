// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws2/remote_view_host/server_remote_view_host.h"

#include <memory>

#include "services/ui/ws2/window_service_test_setup.h"
#include "services/ui/ws2/window_tree.h"
#include "services/ui/ws2/window_tree_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/views/test/test_views_delegate.h"

namespace ui {
namespace ws2 {

TEST(ServerRemoteViewHostTest, EmbedUsingToken) {
  // Use |test_setup| to configure aura and other state.
  WindowServiceTestSetup setup;
  views::TestViewsDelegate views_delegate;

  // Schedule an embed in the tree created by |setup|.
  base::UnguessableToken token;
  const uint32_t window_id_in_child = 149;
  setup.window_tree_test_helper()
      ->window_tree()
      ->ScheduleEmbedForExistingClient(
          window_id_in_child,
          base::BindOnce(
              [](base::UnguessableToken* token,
                 const base::UnguessableToken& actual_token) {
                *token = actual_token;
              },
              &token));
  EXPECT_FALSE(token.is_empty());

  // Create the widget and ServerRemoteViewHost. ServerRemoteViewHost currently
  // won't embed until contained in a Widget.
  // |widget| is deleted by CloseNow() call below.
  views::Widget* widget = views::Widget::CreateWindowWithContext(
      nullptr, setup.aura_test_helper()->root_window());
  // Owned by |widget|.
  ServerRemoteViewHost* remote_view_host =
      new ServerRemoteViewHost(setup.service());
  bool embed_result = false;
  bool embed_result_called = false;
  remote_view_host->EmbedUsingToken(
      token, /* embed_flags */ 0,
      base::BindOnce(
          [](bool* embed_result, bool* embed_result_called,
             bool actual_result) {
            *embed_result = actual_result;
            *embed_result_called = true;
          },
          &embed_result, &embed_result_called));
  // Binding does not happen immediately, only when in a valid Widget.
  EXPECT_FALSE(embed_result_called);

  // Add to the widget, which should trigger the actual embedding.
  widget->GetRootView()->AddChildView(remote_view_host);
  EXPECT_TRUE(embed_result);
  EXPECT_TRUE(embed_result_called);
  widget->CloseNow();
}

}  // namespace ws2
}  // namespace ui
