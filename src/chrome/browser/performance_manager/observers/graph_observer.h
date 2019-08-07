// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_GRAPH_OBSERVER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_GRAPH_OBSERVER_H_

#include "base/macros.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/graph_impl.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/graph/system_node_impl.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"

namespace performance_manager {

// An observer API for the graph.
//
// Observers are generally instantiated when the graph is empty, and outlive it,
// though it's valid for an observer to be registered at any time. Observers
// must unregister before they're destroyed.
//
// To create and install a new observer:
//   (1) Derive from this class.
//   (2) Register by calling on |graph().RegisterObserver|.
//   (3) Before destruction, unregister by calling on
//       |graph().UnregisterObserver|.
//
// NOTE: This interface is deprecated. Please use the individual interfaces
// that this class is implementing.
class GraphObserver : public GraphImpl::Observer,
                      public FrameNodeImpl::Observer,
                      public PageNodeImpl::Observer,
                      public ProcessNodeImpl::Observer,
                      public SystemNodeImpl::Observer {
 public:
  GraphObserver();
  ~GraphObserver() override;

  // Determines whether or not the observer should be registered with, and
  // invoked for, the |node|.
  // TODO(chrisha): Kill this function entirely and delegate that logic to the
  // actual observer implementations.
  virtual bool ShouldObserve(const NodeBase* node) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(GraphObserver);
};

// An empty implementation of the interface.
class GraphObserverDefaultImpl : public GraphObserver {
 public:
  GraphObserverDefaultImpl();
  ~GraphObserverDefaultImpl() override;

  // GraphImplObserver implementation:
  void OnRegistered() override {}
  void OnUnregistered() override {}
  void OnNodeAdded(NodeBase* node) override {}
  void OnBeforeNodeRemoved(NodeBase* node) override {}
  void SetGraph(GraphImpl* graph) override;

  // FrameNodeImplObserver implementation:
  void OnIsCurrentChanged(FrameNodeImpl* frame_node) override {}
  void OnNetworkAlmostIdleChanged(FrameNodeImpl* frame_node) override {}
  void OnLifecycleStateChanged(FrameNodeImpl* frame_node) override {}
  void OnURLChanged(FrameNodeImpl* frame_node) override {}
  void OnNonPersistentNotificationCreated(FrameNodeImpl* frame_node) override {}

  // PageNodeImplObserver implementation:
  void OnIsVisibleChanged(PageNodeImpl* page_node) override {}
  void OnIsLoadingChanged(PageNodeImpl* page_node) override {}
  void OnUkmSourceIdChanged(PageNodeImpl* page_node) override {}
  void OnLifecycleStateChanged(PageNodeImpl* page_node) override {}
  void OnPageAlmostIdleChanged(PageNodeImpl* page_node) override {}
  void OnFaviconUpdated(PageNodeImpl* page_node) override {}
  void OnTitleUpdated(PageNodeImpl* page_node) override {}
  void OnMainFrameNavigationCommitted(PageNodeImpl* page_node) override {}

  // ProcessNodeImplObserver implementation:
  void OnExpectedTaskQueueingDurationSample(
      ProcessNodeImpl* process_node) override {}
  void OnMainThreadTaskLoadIsLow(ProcessNodeImpl* process_node) override {}
  void OnAllFramesInProcessFrozen(ProcessNodeImpl* process_node) override {}

  // SystemNodeImplObserver implementation:
  void OnProcessCPUUsageReady(SystemNodeImpl* system_node) override {}

  GraphImpl* graph() const { return graph_; }

 private:
  GraphImpl* graph_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(GraphObserverDefaultImpl);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_GRAPH_OBSERVER_H_
