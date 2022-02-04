// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "semantic_provider.h"

#include <fuchsia/accessibility/semantics/cpp/fidl.h>
#include <lib/ui/scenic/cpp/view_ref_pair.h>

#include <algorithm>
#include <memory>

#include "base/auto_reset.h"
#include "base/callback.h"
#include "base/fuchsia/process_context.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {
namespace {

using fuchsia::accessibility::semantics::Node;

class AXFuchsiaSemanticProviderDelegate
    : public AXFuchsiaSemanticProvider::Delegate {
 public:
  AXFuchsiaSemanticProviderDelegate() = default;
  ~AXFuchsiaSemanticProviderDelegate() override = default;

  bool OnSemanticsManagerConnectionClosed() override {
    on_semantics_manager_connection_closed_called_ = true;
    return true;
  }

  bool OnAccessibilityAction(
      uint32_t node_id,
      fuchsia::accessibility::semantics::Action action) override {
    on_accessibility_action_called_ = true;
    on_accessibility_action_node_id_ = node_id;
    on_accessibility_action_action_ = std::move(action);
    return true;
  }

  void OnHitTest(fuchsia::math::PointF point,
                 AXFuchsiaSemanticProvider::HitTestCallback callback) override {
    on_hit_test_called_ = true;
    on_hit_test_point_ = std::move(point);
  }

  void OnSemanticsEnabled(bool enabled) override {
    on_semantics_enabled_called_ = true;
  }

  bool on_semantics_manager_connection_closed_called_;
  bool on_accessibility_action_called_;
  uint32_t on_accessibility_action_node_id_ = 10000000;
  fuchsia::accessibility::semantics::Action on_accessibility_action_action_;
  bool on_hit_test_called_;
  fuchsia::math::PointF on_hit_test_point_;
  bool on_semantics_enabled_called_;
};

// Returns a semantic tree of the form:
// (0 (1 2 (3 4 (5))))
std::vector<Node> TreeNodes() {
  Node node_0;
  node_0.set_node_id(0u);
  node_0.set_child_ids({1u, 2u});

  Node node_1;
  node_1.set_node_id(1u);

  Node node_2;
  node_2.set_node_id(2u);
  node_2.set_child_ids({3u, 4u});

  Node node_3;
  node_3.set_node_id(3u);

  Node node_4;
  node_4.set_node_id(4u);
  node_4.set_child_ids({5u});

  Node node_5;
  node_5.set_node_id(5u);

  std::vector<Node> update;
  update.push_back(std::move(node_0));
  update.push_back(std::move(node_1));
  update.push_back(std::move(node_2));
  update.push_back(std::move(node_3));
  update.push_back(std::move(node_4));
  update.push_back(std::move(node_5));
  return update;
}

class AXFuchsiaSemanticProviderTest
    : public ::testing::Test,
      public fuchsia::accessibility::semantics::SemanticsManager,
      public fuchsia::accessibility::semantics::SemanticTree {
 public:
  AXFuchsiaSemanticProviderTest()
      : semantics_manager_bindings_(test_context_.additional_services(), this),
        semantic_tree_binding_(this) {}
  ~AXFuchsiaSemanticProviderTest() override = default;
  AXFuchsiaSemanticProviderTest(const AXFuchsiaSemanticProviderTest&) = delete;
  AXFuchsiaSemanticProviderTest& operator=(
      const AXFuchsiaSemanticProviderTest&) = delete;
  void SetUp() override {
    auto view_ref_pair = scenic::ViewRefPair::New();
    delegate_ = std::make_unique<AXFuchsiaSemanticProviderDelegate>();

    semantic_provider_ = std::make_unique<AXFuchsiaSemanticProvider>(
        std::move(view_ref_pair.view_ref), 2.0f, delegate_.get());

    // Spin the loop to allow registration with the SemanticsManager to be
    // processed.
    base::RunLoop().RunUntilIdle();
  }

 protected:
  // fuchsia::accessibility::semantics::SemanticsManager implementation.
  void RegisterViewForSemantics(
      fuchsia::ui::views::ViewRef view_ref,
      fidl::InterfaceHandle<fuchsia::accessibility::semantics::SemanticListener>
          listener,
      fidl::InterfaceRequest<fuchsia::accessibility::semantics::SemanticTree>
          semantic_tree_request) final {
    semantic_listener_ = listener.Bind();
    semantic_listener_.set_error_handler([](zx_status_t status) {
      // The test should fail if an error occurs.
      ADD_FAILURE();
    });
    semantic_tree_binding_.Bind(std::move(semantic_tree_request));
  }

  // fuchsia::accessibility::semantics::SemanticTree implementation.
  void UpdateSemanticNodes(
      std::vector<fuchsia::accessibility::semantics::Node> nodes) final {
    num_update_semantic_nodes_called_++;
  }
  void DeleteSemanticNodes(std::vector<uint32_t> node_ids) final {
    num_delete_semantic_nodes_called_++;
  }
  void CommitUpdates(CommitUpdatesCallback callback) final { callback(); }
  void SendSemanticEvent(
      fuchsia::accessibility::semantics::SemanticEvent semantic_event,
      SendSemanticEventCallback callback) override {
    callback();
  }

  // Required because of |test_context_|.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  base::TestComponentContextForProcess test_context_;
  // Binding to fake Semantics Manager Fuchsia service, implemented by this test
  // class.
  base::ScopedServiceBinding<
      fuchsia::accessibility::semantics::SemanticsManager>
      semantics_manager_bindings_;

  uint32_t num_update_semantic_nodes_called_ = 0;
  uint32_t num_delete_semantic_nodes_called_ = 0;

  base::RepeatingClosure on_commit_;

  fuchsia::accessibility::semantics::SemanticListenerPtr semantic_listener_;
  fidl::Binding<fuchsia::accessibility::semantics::SemanticTree>
      semantic_tree_binding_;
  std::unique_ptr<AXFuchsiaSemanticProviderDelegate> delegate_;
  std::unique_ptr<AXFuchsiaSemanticProvider> semantic_provider_;
};

TEST_F(AXFuchsiaSemanticProviderTest,
       DISABLED_HandlesOnSemanticsConnectionClosed) {
  semantic_tree_binding_.Close(ZX_ERR_PEER_CLOSED);

  // Spin the loop to allow the channel-close to be handled.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(delegate_->on_semantics_manager_connection_closed_called_);
}

TEST_F(AXFuchsiaSemanticProviderTest, DISABLED_HandlesOnAccessibilityAction) {
  bool action_handled = false;
  semantic_listener_->OnAccessibilityActionRequested(
      /*node_id=*/1u, fuchsia::accessibility::semantics::Action::DEFAULT,
      [&action_handled](bool handled) { action_handled = handled; });

  // Spin the loop to handle the request, and receive the response.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(action_handled);
  EXPECT_TRUE(delegate_->on_accessibility_action_called_);
  EXPECT_EQ(delegate_->on_accessibility_action_node_id_, 1u);
  EXPECT_EQ(delegate_->on_accessibility_action_action_,
            fuchsia::accessibility::semantics::Action::DEFAULT);
}

TEST_F(AXFuchsiaSemanticProviderTest, DISABLED_HandlesOnHitTest) {
  // Note that the point is sent here and will be converted according to the
  // device scale used. Only then it gets sent to the handler, which receives
  // the value already with the proper scaling.
  fuchsia::math::PointF point;
  point.x = 4;
  point.y = 6;
  semantic_listener_->HitTest(std::move(point), [](auto...) {});

  // Spin the loop to allow the call to be processed.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(delegate_->on_hit_test_called_);
  EXPECT_EQ(delegate_->on_hit_test_point_.x, 8.0);
  EXPECT_EQ(delegate_->on_hit_test_point_.y, 12.0);
}

TEST_F(AXFuchsiaSemanticProviderTest, DISABLED_HandlesOnSemanticsEnabled) {
  semantic_listener_->OnSemanticsModeChanged(false, [](auto...) {});

  // Spin the loop to handle the call.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(delegate_->on_semantics_enabled_called_);
}

TEST_F(AXFuchsiaSemanticProviderTest, DISABLED_SendsRootOnly) {
  Node root;
  root.set_node_id(0u);
  EXPECT_TRUE(semantic_provider_->Update(std::move(root)));

  // Spin the loop to process the update call.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(num_update_semantic_nodes_called_, 1u);
  EXPECT_FALSE(semantic_provider_->HasPendingUpdates());
}

TEST_F(AXFuchsiaSemanticProviderTest, DISABLED_SendsNodesFromRootToLeaves) {
  auto tree_nodes = TreeNodes();
  for (auto& node : tree_nodes) {
    EXPECT_TRUE(semantic_provider_->Update(std::move(node)));
  }

  // Spin the loop to process the queued update calls.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(num_update_semantic_nodes_called_, 1u);
  EXPECT_FALSE(semantic_provider_->HasPendingUpdates());
}

TEST_F(AXFuchsiaSemanticProviderTest, DISABLED_SendsNodesFromLeavesToRoot) {
  auto nodes = TreeNodes();
  std::reverse(nodes.begin(), nodes.end());
  for (auto& node : nodes) {
    EXPECT_TRUE(semantic_provider_->Update(std::move(node)));
  }

  // Spin the loop to process the queued update calls.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(num_update_semantic_nodes_called_, 1u);
  EXPECT_FALSE(semantic_provider_->HasPendingUpdates());
}

TEST_F(AXFuchsiaSemanticProviderTest,
       DISABLED_SendsNodesOnlyAfterParentNoLongerPointsToDeletedChild) {
  auto tree_nodes = TreeNodes();
  for (auto& node : tree_nodes) {
    EXPECT_TRUE(semantic_provider_->Update(std::move(node)));
  }

  // Spin the loop to process the queued update calls.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(num_update_semantic_nodes_called_, 1u);
  EXPECT_FALSE(semantic_provider_->HasPendingUpdates());

  // Deletes node 5, which is a child of 4.
  EXPECT_TRUE(semantic_provider_->Delete(5u));

  // Spin the loop to process the deletion call.
  base::RunLoop().RunUntilIdle();

  // Commit is pending, because the parent still points to the child.
  EXPECT_TRUE(semantic_provider_->HasPendingUpdates());

  Node node_4;
  node_4.set_node_id(4u);
  node_4.set_child_ids({});
  EXPECT_TRUE(semantic_provider_->Update(std::move(node_4)));

  // Spin the loop to process the node update.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(num_update_semantic_nodes_called_, 2u);
  EXPECT_EQ(num_delete_semantic_nodes_called_, 1u);

  EXPECT_FALSE(semantic_provider_->HasPendingUpdates());
}

TEST_F(AXFuchsiaSemanticProviderTest,
       DISABLED_SendsNodesOnlyAfterDanglingChildIsDeleted) {
  auto tree_nodes = TreeNodes();
  for (auto& node : tree_nodes) {
    EXPECT_TRUE(semantic_provider_->Update(std::move(node)));
  }

  // Spin the loop to process the queued update calls.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(num_update_semantic_nodes_called_, 1u);
  EXPECT_FALSE(semantic_provider_->HasPendingUpdates());

  Node node_4;
  node_4.set_node_id(4u);
  node_4.set_child_ids({});  // This removes child 5.
  EXPECT_TRUE(semantic_provider_->Update(std::move(node_4)));

  // Spin the loop to process the update call.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(semantic_provider_->HasPendingUpdates());

  EXPECT_TRUE(semantic_provider_->Delete(5u));

  // Spin the loop to process the deletion.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(num_update_semantic_nodes_called_, 2u);
  EXPECT_EQ(num_delete_semantic_nodes_called_, 1u);
  EXPECT_FALSE(semantic_provider_->HasPendingUpdates());
}

TEST_F(AXFuchsiaSemanticProviderTest, DISABLED_ReparentsNodeWithADeletion) {
  auto tree_nodes = TreeNodes();
  for (auto& node : tree_nodes) {
    EXPECT_TRUE(semantic_provider_->Update(std::move(node)));
  }

  // Spin the loop to process the queued update calls.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(num_update_semantic_nodes_called_, 1u);
  EXPECT_FALSE(semantic_provider_->HasPendingUpdates());

  // Deletes node 4 to reparent its child (5).
  EXPECT_TRUE(semantic_provider_->Delete(4u));

  // Spin the loop to process the deletion.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(semantic_provider_->HasPendingUpdates());

  // Add child 5 to another node.
  Node node_1;
  node_1.set_node_id(1u);
  node_1.set_child_ids({5u});
  EXPECT_TRUE(semantic_provider_->Update(std::move(node_1)));

  // Spin the loop to process the update.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(semantic_provider_->HasPendingUpdates());

  Node node_4;
  node_4.set_node_id(4u);
  node_4.set_child_ids({});
  EXPECT_TRUE(semantic_provider_->Update(std::move(node_4)));

  // Spin the loop to process the update.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(num_update_semantic_nodes_called_, 2u);
  EXPECT_EQ(num_delete_semantic_nodes_called_, 1u);
  EXPECT_FALSE(semantic_provider_->HasPendingUpdates());
}

TEST_F(AXFuchsiaSemanticProviderTest, DISABLED_ReparentsNodeWithAnUpdate) {
  auto tree_nodes = TreeNodes();
  for (auto& node : tree_nodes) {
    EXPECT_TRUE(semantic_provider_->Update(std::move(node)));
  }

  // Spin the loop to process the queued update calls.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(num_update_semantic_nodes_called_, 1u);
  EXPECT_FALSE(semantic_provider_->HasPendingUpdates());

  // Add child 5 to another node. Note that 5 will have two parents, and the
  // commit must be held until it has only one.
  Node node_1;
  node_1.set_node_id(1u);
  node_1.set_child_ids({5u});
  EXPECT_TRUE(semantic_provider_->Update(std::move(node_1)));

  // Spin the loop to process the update.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(semantic_provider_->HasPendingUpdates());

  // Updates node 4 to no longer point to 5.
  Node node_4;
  node_4.set_node_id(4u);
  node_4.set_child_ids({});
  EXPECT_TRUE(semantic_provider_->Update(std::move(node_4)));

  // Spin the loop to process the update.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(num_update_semantic_nodes_called_, 2u);
  EXPECT_EQ(num_delete_semantic_nodes_called_, 0u);
  EXPECT_FALSE(semantic_provider_->HasPendingUpdates());
}

TEST_F(AXFuchsiaSemanticProviderTest, DISABLED_ChangesRoot) {
  auto tree_nodes = TreeNodes();
  for (auto& node : tree_nodes) {
    EXPECT_TRUE(semantic_provider_->Update(std::move(node)));
  }

  // Spin the loop to process the queued updated calls.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(num_update_semantic_nodes_called_, 1u);
  EXPECT_FALSE(semantic_provider_->HasPendingUpdates());

  Node new_root;
  new_root.set_node_id(0u);
  new_root.set_child_ids({1u, 2u});
  EXPECT_TRUE(semantic_provider_->Update(std::move(new_root)));

  // Spin the loop to process the update.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(num_update_semantic_nodes_called_, 2u);
  EXPECT_EQ(num_delete_semantic_nodes_called_, 0u);
  EXPECT_FALSE(semantic_provider_->HasPendingUpdates());
}

TEST_F(AXFuchsiaSemanticProviderTest, DISABLED_BatchesUpdates) {
  std::vector<Node> updates;
  for (uint32_t i = 0; i < 30; ++i) {
    Node node;
    node.set_node_id(i);
    node.set_child_ids({i + 1});
    updates.push_back(std::move(node));
  }
  updates.back().clear_child_ids();

  for (auto& node : updates) {
    EXPECT_TRUE(semantic_provider_->Update(std::move(node)));
  }

  // Spin the loop to process the queued update calls.
  base::RunLoop().RunUntilIdle();

  // 30 nodes in batches of 16 (default value of maximum nodes per update call),
  // should result in two update calls to the semantics API.
  EXPECT_EQ(num_update_semantic_nodes_called_, 2u);
  EXPECT_FALSE(semantic_provider_->HasPendingUpdates());
}

TEST_F(AXFuchsiaSemanticProviderTest, DISABLED_ClearsTree) {
  auto tree_nodes = TreeNodes();
  for (auto& node : tree_nodes) {
    EXPECT_TRUE(semantic_provider_->Update(std::move(node)));
  }

  // Spin the loop to process the queued update calls.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(num_update_semantic_nodes_called_, 1u);
  EXPECT_FALSE(semantic_provider_->HasPendingUpdates());

  semantic_provider_->Clear();

  // Spin the loop to process the clear-tree call.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(num_update_semantic_nodes_called_, 1u);
  EXPECT_EQ(num_delete_semantic_nodes_called_, 1u);
  EXPECT_FALSE(semantic_provider_->HasPendingUpdates());
}

}  // namespace
}  // namespace ui
