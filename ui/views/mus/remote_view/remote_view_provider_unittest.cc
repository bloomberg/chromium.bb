// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/remote_view/remote_view_provider.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/unguessable_token.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/mus/window_mus.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/mus/test_window_tree.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tracker.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/mus/remote_view/remote_view_provider_test_api.h"

namespace views {

class TestFocusChangeObserver : public aura::client::FocusChangeObserver {
 public:
  explicit TestFocusChangeObserver(aura::Window* window) : window_(window) {
    aura::client::SetFocusChangeObserver(window_, this);
  }
  ~TestFocusChangeObserver() override {
    aura::client::SetFocusChangeObserver(window_, nullptr);
  }

  // aura::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override {
    on_window_focused_called_ = true;
  }

  bool on_window_focused_called() const { return on_window_focused_called_; }

 private:
  aura::Window* const window_;
  bool on_window_focused_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestFocusChangeObserver);
};

class RemoteViewProviderTest : public aura::test::AuraTestBase {
 public:
  RemoteViewProviderTest() = default;
  ~RemoteViewProviderTest() override = default;

  // aura::test::AuraTestBase
  void SetUp() override {
    env_ = aura::Env::CreateInstance();
    EnableMusWithTestWindowTree();
    AuraTestBase::SetUp();

    test::RemoteViewProviderTestApi::SetWindowTreeClient(
        window_tree_client_impl());

    embedded_ = std::make_unique<aura::Window>(nullptr);
    embedded_->set_owned_by_parent(false);
    embedded_->Init(ui::LAYER_NOT_DRAWN);
    embedded_->SetBounds(gfx::Rect(100, 50));

    provider_ = std::make_unique<RemoteViewProvider>(embedded_.get());
  }

  void TearDown() override {
    // EmbedRoot in |provider_| must be released before WindowTreeClient.
    provider_.reset();

    AuraTestBase::TearDown();
  }

  // Gets the embed token and waits for it.
  base::UnguessableToken GetEmbedToken() {
    base::RunLoop run_loop;
    base::UnguessableToken token;
    provider_->GetEmbedToken(
        base::BindLambdaForTesting([&](const base::UnguessableToken& in_token) {
          token = in_token;
          run_loop.Quit();
        }));
    run_loop.Run();
    return token;
  }

  // Simulates EmbedUsingToken call on embedder side and waits for
  // WindowTreeClient to create a local embedder window.
  aura::Window* SimulateEmbedUsingTokenAndGetEmbedder(
      const base::UnguessableToken& token) {
    base::RunLoop run_loop;
    aura::Window* embedder = nullptr;
    provider_->SetCallbacks(
        base::BindLambdaForTesting([&](aura::Window* in_embedder) {
          embedder = in_embedder;
          run_loop.Quit();
        }),
        base::DoNothing() /* OnUnembedCallback */);
    window_tree()->AddEmbedRootForToken(token);
    run_loop.Run();
    return embedder;
  }

  // Helper to simulate embed.
  aura::Window* SimulateEmbed() {
    base::UnguessableToken token = GetEmbedToken();
    if (token.is_empty()) {
      ADD_FAILURE() << "Failed to get embed token.";
      return nullptr;
    }

    return SimulateEmbedUsingTokenAndGetEmbedder(token);
  }

  // Simulates the embedder window close.
  void SimulateEmbedderClose(aura::Window* embedder) {
    base::RunLoop run_loop;
    provider_->SetCallbacks(
        base::DoNothing() /* OnEmbedCallback */,
        base::BindLambdaForTesting([&]() { run_loop.Quit(); }));

    const ws::Id embedder_window_id =
        aura::WindowMus::Get(embedder)->server_id();
    window_tree()->RemoveEmbedderWindow(embedder_window_id);
    run_loop.Run();
  }

 protected:
  std::unique_ptr<aura::Env> env_;
  std::unique_ptr<aura::Window> embedded_;
  std::unique_ptr<RemoteViewProvider> provider_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoteViewProviderTest);
};

// Tests the basics on the embedded client.
TEST_F(RemoteViewProviderTest, Embed) {
  aura::Window* embedder = SimulateEmbed();
  ASSERT_TRUE(embedder);

  // |embedded_| has the same non-empty size with |embedder| after embed.
  EXPECT_EQ(embedded_->bounds().size(), embedder->bounds().size());
  EXPECT_FALSE(embedded_->bounds().IsEmpty());
  EXPECT_FALSE(embedder->bounds().IsEmpty());

  // |embedded_| resizes with |embedder|.
  const gfx::Rect new_bounds(embedder->bounds().width() + 100,
                             embedder->bounds().height() + 50);
  embedder->SetBounds(new_bounds);
  EXPECT_EQ(embedded_->bounds().size(), embedder->bounds().size());
  EXPECT_FALSE(embedded_->bounds().IsEmpty());
  EXPECT_FALSE(embedder->bounds().IsEmpty());
}

// Tests when |embedded_| is released first.
TEST_F(RemoteViewProviderTest, EmbeddedReleasedFirst) {
  SimulateEmbed();
  embedded_.reset();
}

// Tests when |provider_| is released first.
TEST_F(RemoteViewProviderTest, ClientReleasedFirst) {
  SimulateEmbed();
  provider_.reset();
}

// Tests when embedder goes away first.
TEST_F(RemoteViewProviderTest, EmbedderReleasedFirst) {
  aura::Window* embedder = SimulateEmbed();
  ASSERT_TRUE(embedder);

  SimulateEmbedderClose(embedder);
}

// Tests that the client can embed again.
TEST_F(RemoteViewProviderTest, EmbedAgain) {
  aura::Window* embedder = SimulateEmbed();
  ASSERT_TRUE(embedder);

  aura::WindowTracker window_tracker;
  window_tracker.Add(embedder);
  SimulateEmbedderClose(embedder);
  // SimulateEmbedderClose() should delete |embedder|.
  EXPECT_TRUE(window_tracker.windows().empty());

  aura::Window* new_embedder = SimulateEmbed();
  // SimulateEmbed() should create a new window.
  ASSERT_TRUE(new_embedder);
}

TEST_F(RemoteViewProviderTest, ScreenBounds) {
  aura::Window* embedder = SimulateEmbed();
  ASSERT_TRUE(embedder);

  aura::Window* root_window = embedded_->GetRootWindow();
  ASSERT_TRUE(root_window);
  const gfx::Rect root_bounds(101, 102, 100, 50);
  viz::ParentLocalSurfaceIdAllocator parent_local_surface_id_allocator;
  parent_local_surface_id_allocator.GenerateId();
  window_tree_client()->OnWindowBoundsChanged(
      aura::WindowMus::Get(root_window)->server_id(), gfx::Rect(), root_bounds,
      parent_local_surface_id_allocator.GetCurrentLocalSurfaceIdAllocation());
  EXPECT_EQ(root_bounds, root_window->GetHost()->GetBoundsInPixels());
  EXPECT_EQ(root_bounds.origin(), root_window->GetBoundsInScreen().origin());
}

TEST_F(RemoteViewProviderTest, FocusChangeObserver) {
  SimulateEmbed();

  TestFocusChangeObserver observer(embedded_.get());
  ASSERT_FALSE(observer.on_window_focused_called());

  embedded_->Focus();
  EXPECT_TRUE(observer.on_window_focused_called());
}

TEST_F(RemoteViewProviderTest, Cursor) {
  aura::Window* embedder = SimulateEmbed();
  ASSERT_TRUE(embedder);

  auto* cursor_client =
      aura::client::GetCursorClient(embedded_->GetRootWindow());
  ASSERT_TRUE(cursor_client);

  EXPECT_NE(window_tree()->last_cursor(), ui::CursorType::kHand);
  cursor_client->SetCursor(ui::CursorType::kHand);
  EXPECT_EQ(window_tree()->last_cursor(), ui::CursorType::kHand);
}

}  // namespace views
