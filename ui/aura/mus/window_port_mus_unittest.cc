// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/window_port_mus.h"

#include "cc/mojo_embedder/async_layer_tree_frame_sink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/mus/client_surface_embedder.h"
#include "ui/aura/test/aura_mus_test_base.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/mus/test_window_tree.h"
#include "ui/aura/test/mus/window_port_mus_test_helper.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_features.h"

namespace aura {

using WindowPortMusTest = test::AuraMusClientTestBase;

// TODO(sadrul): https://crbug.com/842361.
TEST_F(WindowPortMusTest,
       DISABLED_LayerTreeFrameSinkGetsCorrectLocalSurfaceId) {
  Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  window.SetBounds(gfx::Rect(300, 300));
  // Notify the window that it will embed an external client, so that it
  // correctly generates LocalSurfaceId.
  window.SetEmbedFrameSinkId(viz::FrameSinkId(0, 1));

  viz::LocalSurfaceId local_surface_id = window.GetLocalSurfaceId();
  ASSERT_TRUE(local_surface_id.is_valid());

  std::unique_ptr<cc::LayerTreeFrameSink> frame_sink(
      window.CreateLayerTreeFrameSink());
  EXPECT_TRUE(frame_sink.get());

  auto mus_frame_sink = WindowPortMusTestHelper(&window).GetFrameSink();
  ASSERT_TRUE(mus_frame_sink);
  auto frame_sink_local_surface_id =
      static_cast<cc::mojo_embedder::AsyncLayerTreeFrameSink*>(
          mus_frame_sink.get())
          ->local_surface_id();
  EXPECT_TRUE(frame_sink_local_surface_id.is_valid());
  EXPECT_EQ(frame_sink_local_surface_id, local_surface_id);
}

TEST_F(WindowPortMusTest, ClientSurfaceEmbedderUpdatesLayer) {
  Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  window.SetBounds(gfx::Rect(300, 300));
  window.SetEmbedFrameSinkId(viz::FrameSinkId(0, 1));

  // Allocate a new LocalSurfaceId. The ui::Layer should be updated.
  window.AllocateLocalSurfaceId();

  auto* window_mus = WindowPortMus::Get(&window);
  viz::LocalSurfaceId local_surface_id = window.GetLocalSurfaceId();
  viz::SurfaceId primary_surface_id =
      window_mus->client_surface_embedder()->GetPrimarySurfaceIdForTesting();
  EXPECT_EQ(local_surface_id, primary_surface_id.local_surface_id());
}

TEST_F(WindowPortMusTest,
       UpdateLocalSurfaceIdFromEmbeddedClientUpdateClientSurfaceEmbedder) {
  Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  window.set_owned_by_parent(false);
  window.SetBounds(gfx::Rect(300, 300));
  // Simulate an embedding.
  window.SetEmbedFrameSinkId(viz::FrameSinkId(0, 1));
  root_window()->AddChild(&window);

  // AckAllChanges() so that can verify a bounds change happens from
  // UpdateLocalSurfaceIdFromEmbeddedClient().
  window_tree()->AckAllChanges();

  // Update the LocalSurfaceId.
  viz::LocalSurfaceId current_id = window.GetSurfaceId().local_surface_id();
  ASSERT_TRUE(current_id.is_valid());
  viz::ParentLocalSurfaceIdAllocator* parent_allocator =
      WindowPortMusTestHelper(&window).GetParentLocalSurfaceIdAllocator();
  parent_allocator->Reset(current_id);
  viz::LocalSurfaceId updated_id = parent_allocator->GenerateId();
  ASSERT_TRUE(updated_id.is_valid());
  EXPECT_NE(updated_id, current_id);
  window.UpdateLocalSurfaceIdFromEmbeddedClient(updated_id);

  // Updating the LocalSurfaceId should propagate to the ClientSurfaceEmbedder.
  auto* window_mus = WindowPortMus::Get(&window);
  ASSERT_TRUE(window_mus);
  ASSERT_TRUE(window_mus->client_surface_embedder());
  EXPECT_EQ(updated_id, window_mus->client_surface_embedder()
                            ->GetPrimarySurfaceIdForTesting()
                            .local_surface_id());

  // The server is notified of a bounds change, so that it sees the new
  // LocalSurfaceId.
  ASSERT_EQ(1u,
            window_tree()->GetChangeCountForType(WindowTreeChangeType::BOUNDS));
  ASSERT_TRUE(window_tree()->last_local_surface_id());
  EXPECT_EQ(window_mus->server_id(), window_tree()->window_id());
  EXPECT_EQ(updated_id, *(window_tree()->last_local_surface_id()));
}

}  // namespace aura
