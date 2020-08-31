// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/decorators/page_aggregator.h"

#include <stdint.h>

#include "components/performance_manager/graph/node_attached_data_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph_operations.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom.h"

namespace performance_manager {

namespace {

const char kDescriberName[] = "PageAggregator";

}

using performance_manager::mojom::InterventionPolicy;

// Provides PageAggregator machinery access to some internals of a PageNodeImpl.
class PageAggregatorAccess {
 public:
  using StorageType = decltype(PageNodeImpl::page_aggregator_data_);

  static StorageType* GetInternalStorage(PageNodeImpl* page_node) {
    return &page_node->page_aggregator_data_;
  }

  static void SetOriginTrialFreezePolicy(PageNodeImpl* page_node,
                                         InterventionPolicy policy) {
    page_node->SetOriginTrialFreezePolicy(policy);
  }

  static void SetPageIsHoldingWebLock(PageNodeImpl* page_node, bool value) {
    page_node->SetIsHoldingWebLock(value);
  }

  static void SetPageIsHoldingIndexedDBLock(PageNodeImpl* page_node,
                                            bool value) {
    page_node->SetIsHoldingIndexedDBLock(value);
  }

  static void SetPageHadFormInteraction(PageNodeImpl* page_node, bool value) {
    page_node->SetHadFormInteraction(value);
  }
};

// Specify the packing alignment for this class as it's expected to have a
// specific size.
#pragma pack(push, 4)
class PageAggregator::Data : public NodeAttachedDataImpl<Data> {
 public:
  using StorageType = PageAggregatorAccess::StorageType;
  struct Traits : public NodeAttachedDataInternalOnNodeType<PageNodeImpl> {};

  explicit Data(const PageNodeImpl* page_node) {}
  ~Data() override {
    DCHECK_EQ(num_frames_holding_web_lock_, 0U);
    DCHECK_EQ(num_frames_holding_indexeddb_lock_, 0U);
    DCHECK_EQ(num_current_frames_with_form_interaction_, 0U);
  }

  static StorageType* GetInternalStorage(PageNodeImpl* page_node) {
    return PageAggregatorAccess::GetInternalStorage(page_node);
  }

  void IncrementFrameCountForFreezingPolicy(InterventionPolicy policy) {
    ++num_current_frames_for_freezing_policy[static_cast<size_t>(policy)];
  }

  void DecrementFrameCountForFreezingPolicy(InterventionPolicy policy) {
    DCHECK_GT(
        num_current_frames_for_freezing_policy[static_cast<size_t>(policy)],
        0U);
    --num_current_frames_for_freezing_policy[static_cast<size_t>(policy)];
  }

  // Updates the counter of frames using WebLocks and sets the corresponding
  // page-level property.
  void UpdateFrameCountForWebLockUsage(bool frame_is_holding_weblock,
                                       PageNodeImpl* page_node);

  // Updates the counter of frames using IndexedDB locks and sets the
  // corresponding page-level property.
  void UpdateFrameCountForIndexedDBLockUsage(
      bool frame_is_holding_indexeddb_lock,
      PageNodeImpl* page_node);

  // Updates the page's origin trial freeze policy from current data.
  void UpdateOriginTrialFreezePolicy(PageNodeImpl* page_node) {
    PageAggregatorAccess::SetOriginTrialFreezePolicy(
        page_node, ComputeOriginTrialFreezePolicy());
  }

  // Updates the counter of frames with form interaction and sets the
  // corresponding page-level property.  |frame_node_being_removed| indicates
  // if this function is called while removing a frame node.
  void UpdateCurrentFrameCountForFormInteraction(
      bool frame_had_form_interaction,
      PageNodeImpl* page_node,
      const FrameNode* frame_node_being_removed);

 private:
  friend class PageAggregator;

  // Computes the page's origin trial freeze policy from current data.
  InterventionPolicy ComputeOriginTrialFreezePolicy() const;

  // Returns the number of current frames with |policy| on the page that owns
  // this Data.
  uint32_t GetNumCurrentFramesForFreezingPolicy(
      InterventionPolicy policy) const {
    return num_current_frames_for_freezing_policy[static_cast<size_t>(policy)];
  }

  // The number of current frames of this page that has set each freeze origin
  // trial policy.
  uint32_t num_current_frames_for_freezing_policy
      [static_cast<size_t>(InterventionPolicy::kMaxValue) + 1] = {};

  // The number of frames holding at least one WebLock / IndexedDB lock. This
  // counts all frames, not just the current ones.
  uint32_t num_frames_holding_web_lock_ = 0;
  uint32_t num_frames_holding_indexeddb_lock_ = 0;

  // The number of current frames which have received some form interaction.
  uint32_t num_current_frames_with_form_interaction_ = 0;

  DISALLOW_COPY_AND_ASSIGN(Data);
};
#pragma pack(pop)

InterventionPolicy PageAggregator::Data::ComputeOriginTrialFreezePolicy()
    const {
  if (GetNumCurrentFramesForFreezingPolicy(InterventionPolicy::kOptOut))
    return InterventionPolicy::kOptOut;

  if (GetNumCurrentFramesForFreezingPolicy(InterventionPolicy::kOptIn))
    return InterventionPolicy::kOptIn;

  // A page with no frame can be frozen. This will have no effect.
  return InterventionPolicy::kDefault;
}

void PageAggregator::Data::UpdateFrameCountForWebLockUsage(
    bool frame_is_holding_weblock,
    PageNodeImpl* page_node) {
  if (frame_is_holding_weblock) {
    ++num_frames_holding_web_lock_;
  } else {
    DCHECK_GT(num_frames_holding_web_lock_, 0U);
    --num_frames_holding_web_lock_;
  }
  PageAggregatorAccess::SetPageIsHoldingWebLock(
      page_node, num_frames_holding_web_lock_ > 0);
}

void PageAggregator::Data::UpdateFrameCountForIndexedDBLockUsage(
    bool frame_is_holding_indexeddb_lock,
    PageNodeImpl* page_node) {
  if (frame_is_holding_indexeddb_lock) {
    ++num_frames_holding_indexeddb_lock_;
  } else {
    DCHECK_GT(num_frames_holding_indexeddb_lock_, 0U);
    --num_frames_holding_indexeddb_lock_;
  }

  PageAggregatorAccess::SetPageIsHoldingIndexedDBLock(
      page_node, num_frames_holding_indexeddb_lock_ > 0);
}

void PageAggregator::Data::UpdateCurrentFrameCountForFormInteraction(
    bool frame_had_form_interaction,
    PageNodeImpl* page_node,
    const FrameNode* frame_node_being_removed) {
  if (frame_had_form_interaction) {
    ++num_current_frames_with_form_interaction_;
  } else {
    DCHECK_GT(num_current_frames_with_form_interaction_, 0U);
    --num_current_frames_with_form_interaction_;
  }
  // DCHECK that the |num_current_frames_with_form_interaction_| accounting is
  // correct.
  DCHECK_EQ(
      [&]() {
        const auto frame_nodes = GraphOperations::GetFrameNodes(page_node);
        size_t num_current_frames_with_form_interaction = 0;
        for (const auto* node : frame_nodes) {
          if (node != frame_node_being_removed && node->IsCurrent() &&
              node->HadFormInteraction()) {
            ++num_current_frames_with_form_interaction;
          }
        }
        return num_current_frames_with_form_interaction;
      }(),
      num_current_frames_with_form_interaction_);

  PageAggregatorAccess::SetPageHadFormInteraction(
      page_node, num_current_frames_with_form_interaction_ > 0);
}

PageAggregator::PageAggregator() = default;
PageAggregator::~PageAggregator() = default;

void PageAggregator::OnFrameNodeAdded(const FrameNode* frame_node) {
  DCHECK(!frame_node->IsCurrent());
}

void PageAggregator::OnBeforeFrameNodeRemoved(const FrameNode* frame_node) {
  auto* page_node = PageNodeImpl::FromNode(frame_node->GetPageNode());
  Data* data = Data::Get(page_node);
  if (frame_node->IsCurrent()) {
    // Data should have been created when the frame became current.
    DCHECK(data);
    data->DecrementFrameCountForFreezingPolicy(
        frame_node->GetOriginTrialFreezePolicy());
    data->UpdateOriginTrialFreezePolicy(page_node);
    if (frame_node->HadFormInteraction()) {
      // Decrement the form interaction counter for this page.
      data->UpdateCurrentFrameCountForFormInteraction(false, page_node,
                                                      frame_node);
    }
  }

  // It is not guaranteed that the graph will be notified that the frame has
  // released its lock before it is notified of the frame being deleted.
  if (frame_node->IsHoldingWebLock())
    data->UpdateFrameCountForWebLockUsage(false, page_node);
  if (frame_node->IsHoldingIndexedDBLock())
    data->UpdateFrameCountForIndexedDBLockUsage(false, page_node);
}

void PageAggregator::OnIsCurrentChanged(const FrameNode* frame_node) {
  auto* page_node = PageNodeImpl::FromNode(frame_node->GetPageNode());
  Data* data = Data::GetOrCreate(page_node);
  if (frame_node->IsCurrent()) {
    data->IncrementFrameCountForFreezingPolicy(
        frame_node->GetOriginTrialFreezePolicy());
  } else {
    data->DecrementFrameCountForFreezingPolicy(
        frame_node->GetOriginTrialFreezePolicy());
  }
  data->UpdateOriginTrialFreezePolicy(page_node);

  // Check if the frame node had some form interaction, in this case there's two
  // possibilities:
  //   - The frame became current: The counter of current frames with form
  //     interactions should be increased.
  //   - The frame became non current: The counter of current frames with form
  //     interactions should be decreased.
  if (frame_node->HadFormInteraction()) {
    data->UpdateCurrentFrameCountForFormInteraction(frame_node->IsCurrent(),
                                                    page_node, nullptr);
  }
}

void PageAggregator::OnOriginTrialFreezePolicyChanged(
    const FrameNode* frame_node,
    const InterventionPolicy& previous_value) {
  if (frame_node->IsCurrent()) {
    auto* page_node = PageNodeImpl::FromNode(frame_node->GetPageNode());
    Data* data = Data::Get(page_node);
    // Data should have been created when the frame became current or the first
    // time this frame grabbed a lock.
    DCHECK(data);
    data->DecrementFrameCountForFreezingPolicy(previous_value);
    data->IncrementFrameCountForFreezingPolicy(
        frame_node->GetOriginTrialFreezePolicy());
    data->UpdateOriginTrialFreezePolicy(page_node);
  }
}

void PageAggregator::OnFrameIsHoldingWebLockChanged(
    const FrameNode* frame_node) {
  auto* page_node = PageNodeImpl::FromNode(frame_node->GetPageNode());
  Data* data = Data::GetOrCreate(page_node);
  data->UpdateFrameCountForWebLockUsage(frame_node->IsHoldingWebLock(),
                                        page_node);
}

void PageAggregator::OnFrameIsHoldingIndexedDBLockChanged(
    const FrameNode* frame_node) {
  auto* page_node = PageNodeImpl::FromNode(frame_node->GetPageNode());
  Data* data = Data::GetOrCreate(page_node);
  data->UpdateFrameCountForIndexedDBLockUsage(
      frame_node->IsHoldingIndexedDBLock(), page_node);
}

void PageAggregator::OnHadFormInteractionChanged(const FrameNode* frame_node) {
  if (frame_node->IsCurrent()) {
    auto* page_node = PageNodeImpl::FromNode(frame_node->GetPageNode());
    Data* data = Data::GetOrCreate(page_node);
    data->UpdateCurrentFrameCountForFormInteraction(
        frame_node->HadFormInteraction(), page_node, nullptr);
  }
}

void PageAggregator::OnPassedToGraph(Graph* graph) {
  // This observer presumes that it's been added before any nodes exist in the
  // graph.
  DCHECK(GraphImpl::FromGraph(graph)->nodes().empty());
  graph->AddFrameNodeObserver(this);
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(this,
                                                           kDescriberName);
}

void PageAggregator::OnTakenFromGraph(Graph* graph) {
  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(this);
  graph->RemoveFrameNodeObserver(this);
}

base::Value PageAggregator::DescribePageNodeData(const PageNode* node) const {
  Data* data = Data::Get(PageNodeImpl::FromNode(node));
  if (data == nullptr)
    return base::Value();

  base::Value ret(base::Value::Type::DICTIONARY);
  ret.SetIntKey("num_frames_holding_web_lock",
                data->num_frames_holding_web_lock_);
  ret.SetIntKey("num_frames_holding_indexeddb_lock",
                data->num_frames_holding_web_lock_);
  ret.SetIntKey("num_current_frames_with_form_interaction",
                data->num_current_frames_with_form_interaction_);
  return ret;
}

}  // namespace performance_manager
