// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/window_port_mus.h"

#include "base/optional.h"
#include "base/run_loop.h"
#include "cc/mojo_embedder/async_layer_tree_frame_sink.h"
#include "components/viz/common/surfaces/child_local_surface_id_allocator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/mus/client_surface_embedder.h"
#include "ui/aura/test/aura_mus_test_base.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/mus/test_window_tree.h"
#include "ui/aura/test/mus/test_window_tree_delegate.h"
#include "ui/aura/test/mus/window_port_mus_test_helper.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_features.h"

namespace aura {

using WindowPortMusTest = test::AuraMusClientTestBase;

namespace {

class TrackOcclusionStateCallWaiter : public TestWindowTreeDelegate {
 public:
  explicit TrackOcclusionStateCallWaiter(TestWindowTree* test_window_tree)
      : test_window_tree_(test_window_tree) {
    test_window_tree_->set_delegate(this);
  }

  ~TrackOcclusionStateCallWaiter() override {
    test_window_tree_->set_delegate(nullptr);
  }

  // Don't wait twice since the Runloop can only be used once.
  void Wait() { run_loop_.Run(); }

  // TestWindowTreeDelegate:
  void TrackOcclusionState(ws::Id window_id) override {
    track_occlusion_state_received_ = true;
    run_loop_.Quit();
  }

  const base::Optional<bool>& track_occlusion_state_call_received() const {
    return track_occlusion_state_received_;
  }

 private:
  base::RunLoop run_loop_;
  TestWindowTree* const test_window_tree_;

  base::Optional<bool> track_occlusion_state_received_;

  DISALLOW_COPY_AND_ASSIGN(TrackOcclusionStateCallWaiter);
};

}  // namespace

// TODO(sadrul): https://crbug.com/842361.
TEST_F(WindowPortMusTest,
       DISABLED_LayerTreeFrameSinkGetsCorrectLocalSurfaceId) {
  Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  window.SetBounds(gfx::Rect(300, 300));
  // Notify the window that it will embed an external client, so that it
  // correctly generates LocalSurfaceId.
  window.SetEmbedFrameSinkId(viz::FrameSinkId(0, 1));

  viz::LocalSurfaceId local_surface_id =
      window.GetLocalSurfaceIdAllocation().local_surface_id();
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
  WindowPortMusTestHelper(&window).SimulateEmbedding();

  // Allocate a new LocalSurfaceId. The ui::Layer should be updated.
  window.AllocateLocalSurfaceId();

  viz::LocalSurfaceId local_surface_id =
      window.GetLocalSurfaceIdAllocation().local_surface_id();
  ClientSurfaceEmbedder* client_surface_embedder =
      WindowPortMusTestHelper(&window).GetClientSurfaceEmbedder();
  ASSERT_TRUE(client_surface_embedder);
  viz::SurfaceId primary_surface_id = client_surface_embedder->GetSurfaceId();
  EXPECT_EQ(local_surface_id, primary_surface_id.local_surface_id());
}

TEST_F(WindowPortMusTest,
       UpdateLocalSurfaceIdFromEmbeddedClientUpdateClientSurfaceEmbedder) {
  Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  window.set_owned_by_parent(false);
  window.SetBounds(gfx::Rect(300, 300));
  // Simulate an embedding.
  WindowPortMusTestHelper(&window).SimulateEmbedding();
  root_window()->AddChild(&window);

  // AckAllChanges() so that can verify a bounds change happens from
  // UpdateLocalSurfaceIdFromEmbeddedClient().
  window_tree()->AckAllChanges();

  // Update the LocalSurfaceId.
  viz::LocalSurfaceId current_id = window.GetSurfaceId().local_surface_id();
  ASSERT_TRUE(current_id.is_valid());
  viz::ParentLocalSurfaceIdAllocator* parent_allocator =
      WindowPortMusTestHelper(&window).GetParentLocalSurfaceIdAllocator();
  parent_allocator->GenerateId();
  const viz::LocalSurfaceId& updated_id =
      parent_allocator->GetCurrentLocalSurfaceIdAllocation().local_surface_id();
  ASSERT_TRUE(updated_id.is_valid());
  EXPECT_NE(updated_id, current_id);
  window.UpdateLocalSurfaceIdFromEmbeddedClient(
      parent_allocator->GetCurrentLocalSurfaceIdAllocation());

  // Updating the LocalSurfaceId should propagate to the ClientSurfaceEmbedder.
  ClientSurfaceEmbedder* client_surface_embedder =
      WindowPortMusTestHelper(&window).GetClientSurfaceEmbedder();
  ASSERT_TRUE(client_surface_embedder);
  EXPECT_EQ(updated_id,
            client_surface_embedder->GetSurfaceId().local_surface_id());

  // The server is notified of a bounds change, so that it sees the new
  // LocalSurfaceId.
  ASSERT_EQ(1u,
            window_tree()->GetChangeCountForType(WindowTreeChangeType::BOUNDS));
  ASSERT_TRUE(window_tree()->last_local_surface_id());
  auto* window_mus = WindowPortMus::Get(&window);
  ASSERT_TRUE(window_mus);
  EXPECT_EQ(window_mus->server_id(), window_tree()->window_id());
  EXPECT_EQ(updated_id, *(window_tree()->last_local_surface_id()));
}

// Tests that Window::TrackOcclusionState calls into WindowTree under mus.
TEST_F(WindowPortMusTest, TrackOcclusionState) {
  Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  window.SetBounds(gfx::Rect(400, 300));

  TrackOcclusionStateCallWaiter waiter(window_tree());
  window.TrackOcclusionState();
  waiter.Wait();
  EXPECT_TRUE(waiter.track_occlusion_state_call_received() == true);
}

TEST_F(WindowPortMusTest, LocalOcclusionStateFromVisibility) {
  Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  window.set_owned_by_parent(false);
  window.SetBounds(gfx::Rect(400, 300));

  root_window()->AddChild(&window);
  window.TrackOcclusionState();
  ASSERT_EQ(Window::OcclusionState::HIDDEN, window.occlusion_state());

  // Use a dummy waiter to disable simulated WindowOcclusionTracker behavior.
  TrackOcclusionStateCallWaiter waiter(window_tree());

  // Single window case.
  window.Show();
  EXPECT_EQ(Window::OcclusionState::VISIBLE, window.occlusion_state());

  window.Hide();
  EXPECT_EQ(Window::OcclusionState::HIDDEN, window.occlusion_state());

  // Window has a parent.
  Window parent(nullptr);
  parent.Init(ui::LAYER_NOT_DRAWN);
  parent.set_owned_by_parent(false);
  parent.SetBounds(gfx::Rect(400, 300));
  root_window()->AddChild(&parent);
  parent.AddChild(&window);

  // Stays HIDDEN because parent is not shown.
  window.Show();
  EXPECT_EQ(Window::OcclusionState::HIDDEN, window.occlusion_state());

  // Changes to VISIBLE when window.IsVisible() is true.
  parent.Show();
  EXPECT_EQ(Window::OcclusionState::VISIBLE, window.occlusion_state());

  // Changes to HIDDEN when parent hides.
  parent.Hide();
  EXPECT_EQ(Window::OcclusionState::HIDDEN, window.occlusion_state());
}

TEST_F(WindowPortMusTest, PrepareForEmbed) {
  Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  window.set_owned_by_parent(false);
  window.SetBounds(gfx::Rect(400, 300));

  WindowPortMusTestHelper helper(&window);
  helper.SimulateEmbedding();
  ClientSurfaceEmbedder* client_surface_embedder =
      WindowPortMusTestHelper(&window).GetClientSurfaceEmbedder();
  ASSERT_TRUE(client_surface_embedder);
  EXPECT_TRUE(client_surface_embedder->GetSurfaceId().is_valid());
}

class TestDragDropDelegate : public client::DragDropDelegate {
 public:
  TestDragDropDelegate() = default;
  ~TestDragDropDelegate() override = default;

  // client::DragDropDelegate:
  void OnDragEntered(const ui::DropTargetEvent& event) override {}
  int OnDragUpdated(const ui::DropTargetEvent& event) override { return 0; }
  void OnDragExited() override {}
  int OnPerformDrop(const ui::DropTargetEvent& event) override { return 0; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestDragDropDelegate);
};

TEST_F(WindowPortMusTest, CanAcceptDrops) {
  TestDragDropDelegate test_delegate;

  Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  window.set_owned_by_parent(false);
  window.SetBounds(gfx::Rect(400, 300));

  EXPECT_EQ(0u, window_tree()->get_and_clear_accepts_drops_count());

  // Setting the DragDropDelegate should implicitly call
  // SetCanAcceptDrops(true).
  client::SetDragDropDelegate(&window, &test_delegate);
  EXPECT_EQ(1u, window_tree()->get_and_clear_accepts_drops_count());
  EXPECT_TRUE(window_tree()->last_accepts_drops());

  // And removing the DragDropDelegate should implicitly call
  // SetCanAcceptDrops(false).
  client::SetDragDropDelegate(&window, nullptr);
  EXPECT_EQ(1u, window_tree()->get_and_clear_accepts_drops_count());
  EXPECT_FALSE(window_tree()->last_accepts_drops());
}

TEST_F(WindowPortMusTest, RegisterFrameSinkId) {
  Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  window.set_owned_by_parent(false);
  window.SetBounds(gfx::Rect(400, 300));

  root_window()->AddChild(&window);
  window_tree()->AckAllChanges();
  window.SetEmbedFrameSinkId(viz::FrameSinkId(0, 1));

  // Setting a FrameSinkId should trigger generating LocalSurfaceIds.
  ASSERT_EQ(1u,
            window_tree()->GetChangeCountForType(WindowTreeChangeType::BOUNDS));
  ASSERT_TRUE(window_tree()->last_local_surface_id());
  EXPECT_EQ(window_tree()->last_local_surface_id(),
            window.GetLocalSurfaceIdAllocation().local_surface_id());
  auto local_surface_id =
      window.GetLocalSurfaceIdAllocation().local_surface_id();
  window_tree()->AckAllChanges();

  // Changing the bounds should trigger a new LocalSurfaceId.
  window.SetBounds(gfx::Rect(400, 310));
  ASSERT_EQ(1u,
            window_tree()->GetChangeCountForType(WindowTreeChangeType::BOUNDS));
  ASSERT_TRUE(window_tree()->last_local_surface_id());
  EXPECT_EQ(window_tree()->last_local_surface_id(),
            window.GetLocalSurfaceIdAllocation().local_surface_id());
  EXPECT_NE(local_surface_id,
            window.GetLocalSurfaceIdAllocation().local_surface_id());
}

}  // namespace aura
