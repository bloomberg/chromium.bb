// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/decorators/frozen_frame_aggregator.h"

#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/graph_impl.h"
#include "chrome/browser/performance_manager/graph/node_attached_data_impl.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"

namespace performance_manager {

using LifecycleState = resource_coordinator::mojom::LifecycleState;

// Provides FrozenFrameAggregator machinery access to some internals of a
// PageNodeImpl and ProcessNodeImpl.
class FrozenFrameAggregatorAccess {
 public:
  using StorageType = decltype(PageNodeImpl::frozen_frame_data_);

  static StorageType* GetInternalStorage(PageNodeImpl* page_node) {
    return &page_node->frozen_frame_data_;
  }

  static StorageType* GetInternalStorage(ProcessNodeImpl* process_node) {
    return &process_node->frozen_frame_data_;
  }

  static void SetLifecycleState(PageNodeImpl* page_node,
                                LifecycleState lifecycle_state) {
    page_node->SetLifecycleState(lifecycle_state);
  }

  static void NotifyAllFramesInProcessFrozen(ProcessNodeImpl* process_node) {
    for (auto& observer : process_node->observers())
      observer.OnAllFramesInProcessFrozen(process_node);
  }
};

namespace {

// Private implementation of the node attached data. This keeps the complexity
// out of the header file.
class FrozenDataImpl : public FrozenFrameAggregator::Data,
                       public NodeAttachedDataImpl<FrozenDataImpl> {
 public:
  using StorageType = FrozenFrameAggregatorAccess::StorageType;

  // This data is tracked persistently for page and process nodes, so uses
  // internal node storage.
  struct Traits : public NodeAttachedDataInternalOnNodeType<PageNodeImpl>,
                  public NodeAttachedDataInternalOnNodeType<ProcessNodeImpl> {};

  explicit FrozenDataImpl(const PageNodeImpl* page_node) {}
  explicit FrozenDataImpl(const ProcessNodeImpl* process_node) {}
  ~FrozenDataImpl() override = default;

  static StorageType* GetInternalStorage(PageNodeImpl* page_node) {
    return FrozenFrameAggregatorAccess::GetInternalStorage(page_node);
  }

  static StorageType* GetInternalStorage(ProcessNodeImpl* process_node) {
    return FrozenFrameAggregatorAccess::GetInternalStorage(process_node);
  }

  // Returns the current "is_frozen" state. A collection of frames is considered
  // frozen if its non-empty, and all of the frames are frozen.
  bool IsFrozen() const {
    return current_frame_count > 0 && frozen_frame_count == current_frame_count;
  }

  // Returns the state as an equivalent LifecycleState.
  LifecycleState AsLifecycleState() const {
    if (IsFrozen())
      return LifecycleState::kFrozen;
    return LifecycleState::kRunning;
  }

  // Applies a change to frame counts. Returns true if that causes the frozen
  // state to change for this object.
  bool ChangeFrameCounts(int32_t current_frame_delta,
                         int32_t frozen_frame_delta) {
    DCHECK(current_frame_delta != 0 || frozen_frame_delta != 0);
    DCHECK_GE(1, abs(current_frame_delta));
    DCHECK_GE(1, abs(frozen_frame_delta));
    // We should never have (-1, 1) or (1, -1).
    DCHECK_NE(-current_frame_delta, frozen_frame_delta);

    // If the deltas are negative, the counts need to be positive.
    DCHECK(current_frame_delta >= 0 || current_frame_count > 0);
    DCHECK(frozen_frame_delta >= 0 || frozen_frame_count > 0);

    bool was_frozen = IsFrozen();
    current_frame_count += current_frame_delta;
    frozen_frame_count += frozen_frame_delta;

    return IsFrozen() != was_frozen;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FrozenDataImpl);
};

bool IsFrozen(const FrameNodeImpl* frame_node) {
  return frame_node->lifecycle_state() == LifecycleState::kFrozen;
}

}  // namespace

FrozenFrameAggregator::FrozenFrameAggregator() = default;
FrozenFrameAggregator::~FrozenFrameAggregator() = default;

void FrozenFrameAggregator::OnRegistered() {
  // This observer presumes that it's been added before any nodes exist in the
  // graph.
  DCHECK(graph()->nodes().empty());
}

bool FrozenFrameAggregator::ShouldObserve(const NodeBase* node) {
  // Use the ShouldObserve hook to ensure page and process node attached data
  // is initialized. There's no need to observe these nodes beyond that.
  switch (node->type()) {
    case FrameNodeImpl::Type():
      return true;

    case PageNodeImpl::Type(): {
      auto* page_node = PageNodeImpl::FromNodeBase(node);
      // Expect a page to always start in the running state.
      DCHECK_EQ(LifecycleState::kRunning, page_node->lifecycle_state());
      FrozenDataImpl::GetOrCreate(page_node);
      return false;
    }

    case ProcessNodeImpl::Type(): {
      FrozenDataImpl::GetOrCreate(ProcessNodeImpl::FromNodeBase(node));
      return false;
    }

    default:
      return false;
  }
  NOTREACHED();
}

void FrozenFrameAggregator::OnNodeAdded(NodeBase* node) {
  // We only observe frame nodes.
  DCHECK_EQ(FrameNodeImpl::Type(), node->type());

  auto* frame_node = FrameNodeImpl::FromNodeBase(node);
  DCHECK(!IsFrozen(frame_node));  // A newly created node can never be frozen.
  AddOrRemoveFrame(frame_node, 1);
}

void FrozenFrameAggregator::OnBeforeNodeRemoved(NodeBase* node) {
  if (node->type() != FrameNodeImpl::Type())
    return;

  auto* frame_node = FrameNodeImpl::FromNodeBase(node);
  AddOrRemoveFrame(frame_node, -1);
}

void FrozenFrameAggregator::OnIsCurrentChanged(FrameNodeImpl* frame_node) {
  int32_t current_frame_delta = frame_node->is_current() ? 1 : -1;
  int32_t frozen_frame_delta = IsFrozen(frame_node) ? current_frame_delta : 0;
  UpdateFrameCounts(frame_node, current_frame_delta, frozen_frame_delta);
}

void FrozenFrameAggregator::OnLifecycleStateChanged(FrameNodeImpl* frame_node) {
  if (!frame_node->is_current())
    return;
  int32_t frozen_frame_delta = IsFrozen(frame_node) ? 1 : -1;
  UpdateFrameCounts(frame_node, 0, frozen_frame_delta);
}

void FrozenFrameAggregator::AddOrRemoveFrame(FrameNodeImpl* frame_node,
                                             int32_t delta) {
  int32_t current_frame_delta = 0;
  int32_t frozen_frame_delta = 0;
  if (frame_node->is_current()) {
    current_frame_delta = delta;
    if (IsFrozen(frame_node))
      frozen_frame_delta = delta;
  }

  UpdateFrameCounts(frame_node, current_frame_delta, frozen_frame_delta);
}

void FrozenFrameAggregator::UpdateFrameCounts(FrameNodeImpl* frame_node,
                                              int32_t current_frame_delta,
                                              int32_t frozen_frame_delta) {
  // If a non-current frame is added or removed the deltas can be zero. In this
  // case the logic can be aborted early to save some effort.
  if (current_frame_delta == 0 && frozen_frame_delta == 0)
    return;

  auto* page_node = frame_node->page_node();
  auto* process_node = frame_node->process_node();
  auto* page_data = FrozenDataImpl::Get(page_node);
  auto* process_data = FrozenDataImpl::Get(process_node);

  // Set the page lifecycle state based on the state of the frame tree.
  if (page_data->ChangeFrameCounts(current_frame_delta, frozen_frame_delta)) {
    FrozenFrameAggregatorAccess::SetLifecycleState(
        page_node, page_data->AsLifecycleState());
  }

  // Update the process state, and notify when all frames in the tree are
  // frozen.
  if (process_data->ChangeFrameCounts(current_frame_delta,
                                      frozen_frame_delta) &&
      process_data->IsFrozen()) {
    FrozenFrameAggregatorAccess::NotifyAllFramesInProcessFrozen(process_node);
  }
}

// static
FrozenFrameAggregator::Data* FrozenFrameAggregator::Data::GetForTesting(
    PageNodeImpl* page_node) {
  return FrozenDataImpl::Get(page_node);
}

// static
FrozenFrameAggregator::Data* FrozenFrameAggregator::Data::GetForTesting(
    ProcessNodeImpl* process_node) {
  return FrozenDataImpl::Get(process_node);
}

}  // namespace performance_manager
