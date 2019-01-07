// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/window_delegate_impl.h"

#include "services/ws/embedding.h"
#include "services/ws/test_window_service_delegate.h"
#include "services/ws/window_service_test_setup.h"
#include "services/ws/window_tree_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_type.h"
#include "ui/gfx/geometry/point.h"

namespace ws {

TEST(WindowDeleteImplTest, GetCursorTopLevel) {
  WindowServiceTestSetup setup;
  // WindowDelegateImpl deletes itself when the window is deleted.
  WindowDelegateImpl* delegate = new WindowDelegateImpl();
  setup.delegate()->set_delegate_for_next_top_level(delegate);
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  delegate->set_window(top_level);
  // Verify no crash when no cursor has been set.
  top_level->GetCursor(gfx::Point());

  // Set a cursor and ensure we get it back.
  const ui::Cursor help_cursor(ui::CursorType::kHelp);
  setup.window_tree_test_helper()->SetCursor(top_level, help_cursor);
  EXPECT_EQ(help_cursor.native_type(),
            top_level->GetCursor(gfx::Point()).native_type());
}

TEST(WindowDeleteImplTest, GetCursorForEmbedding) {
  WindowServiceTestSetup setup;
  // WindowDelegateImpl deletes itself when the window is deleted.
  WindowDelegateImpl* delegate = new WindowDelegateImpl();
  setup.delegate()->set_delegate_for_next_top_level(delegate);
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  delegate->set_window(top_level);

  // Create an embedding.
  aura::Window* embed_window = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(embed_window);
  top_level->AddChild(embed_window);
  std::unique_ptr<EmbeddingHelper> embedding_helper =
      setup.CreateEmbedding(embed_window);

  // Set a cursor on the embedded window from the embedded client and ensure we
  // get it back.
  const ui::Cursor help_cursor(ui::CursorType::kHelp);
  embedding_helper->window_tree_test_helper->SetCursor(embed_window,
                                                       help_cursor);
  EXPECT_EQ(help_cursor.native_type(),
            embed_window->GetCursor(gfx::Point()).native_type());
}

TEST(WindowDeleteImplTest, GetCursorForEmbeddingInterceptsEvents) {
  WindowServiceTestSetup setup;
  // WindowDelegateImpl deletes itself when the window is deleted.
  WindowDelegateImpl* delegate = new WindowDelegateImpl();
  setup.delegate()->set_delegate_for_next_top_level(delegate);
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  delegate->set_window(top_level);

  // Set a cursor on |top_level|.
  const ui::Cursor help_cursor(ui::CursorType::kHelp);
  setup.window_tree_test_helper()->SetCursor(top_level, help_cursor);

  // Create an embedding.
  aura::Window* embed_window = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(embed_window);
  top_level->AddChild(embed_window);
  std::unique_ptr<EmbeddingHelper> embedding_helper = setup.CreateEmbedding(
      embed_window, mojom::kEmbedFlagEmbedderInterceptsEvents);

  // Set a cursor on the embedding. Because the embedding was created with
  // kEmbedFlagEmbedderInterceptsEvents the cursor should come from the parent
  // (|top_level|).
  const ui::Cursor ibeam_cursor(ui::CursorType::kIBeam);
  embedding_helper->window_tree_test_helper->SetCursor(embed_window,
                                                       ibeam_cursor);
  EXPECT_EQ(help_cursor.native_type(),
            embed_window->GetCursor(gfx::Point()).native_type());
}

}  // namespace ws
