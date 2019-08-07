// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_GRAPH_TEST_HARNESS_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_GRAPH_TEST_HARNESS_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <utility>

#include "base/test/scoped_task_environment.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/graph.h"
#include "chrome/browser/performance_manager/graph/node_base.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/graph/system_node_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

class SystemNodeImpl;

template <class NodeClass>
class TestNodeWrapper {
 public:
  template <typename... Args>
  static TestNodeWrapper<NodeClass> Create(Graph* graph, Args&&... args) {
    std::unique_ptr<NodeClass> node =
        std::make_unique<NodeClass>(graph, std::forward<Args>(args)...);
    graph->AddNewNode(node.get());
    return TestNodeWrapper<NodeClass>(std::move(node));
  }

  TestNodeWrapper() {}

  explicit TestNodeWrapper(std::unique_ptr<NodeClass> impl)
      : impl_(std::move(impl)) {
    DCHECK(impl_.get());
  }

  TestNodeWrapper(TestNodeWrapper&& other) : impl_(std::move(other.impl_)) {}

  void operator=(TestNodeWrapper&& other) { impl_ = std::move(other.impl_); }
  void operator=(TestNodeWrapper& other) = delete;

  ~TestNodeWrapper() { reset(); }

  NodeClass* operator->() const { return impl_.get(); }

  NodeClass* get() const { return impl_.get(); }

  void reset() {
    if (impl_) {
      impl_->graph()->RemoveNode(impl_.get());
      impl_.reset();
    }
  }

 private:
  std::unique_ptr<NodeClass> impl_;
};

// This specialization is necessary because the graph has ownership of the
// system node as it's a singleton. For the other node types the test wrapper
// manages the node lifetime.
template <>
class TestNodeWrapper<SystemNodeImpl> {
 public:
  static TestNodeWrapper<SystemNodeImpl> Create(Graph* graph) {
    return TestNodeWrapper<SystemNodeImpl>(graph->FindOrCreateSystemNode());
  }

  explicit TestNodeWrapper(SystemNodeImpl* impl) : impl_(impl) {}
  TestNodeWrapper(TestNodeWrapper&& other) : impl_(other.impl_) {}
  ~TestNodeWrapper() { reset(); }

  SystemNodeImpl* operator->() const { return impl_; }
  SystemNodeImpl* get() const { return impl_; }

  void reset() { impl_ = nullptr; }

 private:
  SystemNodeImpl* impl_;

  DISALLOW_COPY_AND_ASSIGN(TestNodeWrapper);
};

class GraphTestHarness : public ::testing::Test {
 public:
  GraphTestHarness();
  ~GraphTestHarness() override;

  template <class NodeClass, typename... Args>
  TestNodeWrapper<NodeClass> CreateNode(Args&&... args) {
    return TestNodeWrapper<NodeClass>::Create(graph(),
                                              std::forward<Args>(args)...);
  }

  TestNodeWrapper<SystemNodeImpl> GetSystemCoordinationUnit() {
    return TestNodeWrapper<SystemNodeImpl>(graph()->FindOrCreateSystemNode());
  }

  // testing::Test:
  void TearDown() override;

 protected:
  base::test::ScopedTaskEnvironment& task_env() { return task_env_; }
  Graph* graph() { return &graph_; }

 private:
  base::test::ScopedTaskEnvironment task_env_;
  Graph graph_;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_GRAPH_TEST_HARNESS_H_
