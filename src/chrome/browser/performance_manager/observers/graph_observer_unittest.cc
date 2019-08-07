// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/observers/graph_observer.h"

#include <memory>

#include "base/process/process_handle.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/graph_impl.h"
#include "chrome/browser/performance_manager/graph/graph_test_harness.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

class GraphObserverTest : public GraphTestHarness {};

class TestGraphObserver : public GraphObserverDefaultImpl {
 public:
  TestGraphObserver() {}
  ~TestGraphObserver() override {}

  size_t node_created_count() const { return node_created_count_; }
  size_t node_destroyed_count() const { return node_destroyed_count_; }
  size_t registered() const { return registered_; }
  size_t unregistered() const { return unregistered_; }

  // Overridden from GraphObserver.
  void OnRegistered() override { ++registered_; }
  void OnUnregistered() override { ++unregistered_; }
  bool ShouldObserve(const NodeBase* node) override {
    return node->type() == FrameNodeImpl::Type();
  }
  void OnNodeAdded(NodeBase* node) override { ++node_created_count_; }
  void OnBeforeNodeRemoved(NodeBase* node) override { ++node_destroyed_count_; }

 private:
  size_t node_created_count_ = 0;
  size_t node_destroyed_count_ = 0;
  size_t registered_ = 0;
  size_t unregistered_ = 0;
};

}  // namespace

TEST_F(GraphObserverTest, CallbacksInvoked) {
  EXPECT_TRUE(graph()->observers_for_testing().empty());
  auto observer = std::make_unique<TestGraphObserver>();
  EXPECT_EQ(0u, observer->registered());
  graph()->RegisterObserver(observer.get());
  EXPECT_EQ(1u, observer->registered());
  EXPECT_EQ(1u, graph()->observers_for_testing().size());


  {
    auto process_node = CreateNode<ProcessNodeImpl>();
    auto page_node = CreateNode<PageNodeImpl>();
    auto root_frame_node =
        CreateNode<FrameNodeImpl>(process_node.get(), page_node.get());
    auto frame_node = CreateNode<FrameNodeImpl>(
        process_node.get(), page_node.get(), root_frame_node.get(), 1);

    EXPECT_EQ(2u, observer->node_created_count());
  }

  EXPECT_EQ(2u, observer->node_destroyed_count());

  EXPECT_EQ(0u, observer->unregistered());
  graph()->UnregisterObserver(observer.get());
  EXPECT_EQ(1u, observer->unregistered());
}

}  // namespace performance_manager
