// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/embedding.h"

#include <stdint.h>

#include <memory>
#include <queue>

#include "services/ws/public/mojom/window_tree_constants.mojom.h"
#include "services/ws/window_service.h"
#include "services/ws/window_service_test_setup.h"
#include "services/ws/window_tree_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tracker.h"

namespace ws {
namespace {

TEST(EmbeddingTest, DestroyingRootDestroysEmbedding) {
  WindowServiceTestSetup setup;
  aura::Window* embed_window = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(embed_window);
  std::unique_ptr<EmbeddingHelper> embedding_helper =
      setup.CreateEmbedding(embed_window);
  ASSERT_TRUE(embedding_helper);
  aura::Window* embed_child_window =
      embedding_helper->window_tree_test_helper->NewWindow();
  ASSERT_TRUE(embed_child_window);
  aura::WindowTracker tracker;
  tracker.Add(embed_child_window);

  setup.window_tree_test_helper()->DeleteWindow(embed_window);
  // Deleting the |embed_window| deletes the embedded client and anything it
  // created, which is |embed_child_window|.
  EXPECT_TRUE(tracker.windows().empty());
  // Deleting |embed_window| should delete the embedding. Reset |embedding|
  // to prevent EmbeddingHelper from attempting to delete the Embedding too.
  embedding_helper->embedding = nullptr;
}

}  // namespace
}  // namespace ws
