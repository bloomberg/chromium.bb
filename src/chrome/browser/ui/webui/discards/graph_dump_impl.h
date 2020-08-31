// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DISCARDS_GRAPH_DUMP_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_DISCARDS_GRAPH_DUMP_IMPL_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/util/type_safety/id_type.h"
#include "chrome/browser/ui/webui/discards/discards.mojom.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/graph/worker_node.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class DiscardsGraphDumpImpl : public discards::mojom::GraphDump,
                              public performance_manager::GraphOwned,
                              public performance_manager::FrameNodeObserver,
                              public performance_manager::PageNodeObserver,
                              public performance_manager::ProcessNodeObserver,
                              public performance_manager::WorkerNodeObserver {
 public:
  DiscardsGraphDumpImpl();
  ~DiscardsGraphDumpImpl() override;

  // Creates a new DiscardsGraphDumpImpl to service |receiver| and passes its
  // ownership to |graph|.
  static void CreateAndBind(
      mojo::PendingReceiver<discards::mojom::GraphDump> receiver,
      performance_manager::Graph* graph);

  // Exposed for testing.
  void BindWithGraph(
      performance_manager::Graph* graph,
      mojo::PendingReceiver<discards::mojom::GraphDump> receiver);

  int64_t GetNodeIdForTesting(const performance_manager::Node* node);

 protected:
  // WebUIGraphDump implementation.
  void SubscribeToChanges(
      mojo::PendingRemote<discards::mojom::GraphChangeStream> change_subscriber)
      override;
  void RequestNodeDescriptions(
      const std::vector<int64_t>& node_ids,
      RequestNodeDescriptionsCallback callback) override;

  // GraphOwned implementation.
  void OnPassedToGraph(performance_manager::Graph* graph) override;
  void OnTakenFromGraph(performance_manager::Graph* graph) override;

  // FrameNodeObserver implementation:
  void OnFrameNodeAdded(
      const performance_manager::FrameNode* frame_node) override;
  void OnBeforeFrameNodeRemoved(
      const performance_manager::FrameNode* frame_node) override;
  // Ignored.
  void OnIsCurrentChanged(
      const performance_manager::FrameNode* frame_node) override {}
  // Ignored.
  void OnNetworkAlmostIdleChanged(
      const performance_manager::FrameNode* frame_node) override {}
  // Ignored.
  void OnFrameLifecycleStateChanged(
      const performance_manager::FrameNode* frame_node) override {}
  // Ignored.
  void OnOriginTrialFreezePolicyChanged(
      const performance_manager::FrameNode* frame_node,
      const InterventionPolicy& previous_value) override {}
  void OnURLChanged(const performance_manager::FrameNode* frame_node,
                    const GURL& previous_value) override;
  // Ignored.
  void OnIsAdFrameChanged(
      const performance_manager::FrameNode* frame_node) override {}
  // Ignored.
  void OnFrameIsHoldingWebLockChanged(
      const performance_manager::FrameNode* frame_node) override {}
  // Ignored.
  void OnFrameIsHoldingIndexedDBLockChanged(
      const performance_manager::FrameNode* frame_node) override {}
  // Ignored.
  void OnNonPersistentNotificationCreated(
      const performance_manager::FrameNode* frame_node) override {}
  // Ignored.
  void OnPriorityAndReasonChanged(
      const performance_manager::FrameNode* frame_node,
      const PriorityAndReason& previous_value) override {}
  // Ignored.
  void OnHadFormInteractionChanged(
      const performance_manager::FrameNode* frame_node) override {}
  // Ignored.
  void OnFirstContentfulPaint(
      const performance_manager::FrameNode* frame_node,
      base::TimeDelta time_since_navigation_start) override {}

  // PageNodeObserver implementation:
  void OnPageNodeAdded(const performance_manager::PageNode* page_node) override;
  void OnBeforePageNodeRemoved(
      const performance_manager::PageNode* page_node) override;
  void OnIsVisibleChanged(
      const performance_manager::PageNode* page_node) override {}  // Ignored.
  void OnIsAudibleChanged(
      const performance_manager::PageNode* page_node) override {}  // Ignored.
  void OnIsLoadingChanged(
      const performance_manager::PageNode* page_node) override {}  // Ignored.
  void OnUkmSourceIdChanged(
      const performance_manager::PageNode* page_node) override {}  // Ignored.
  // Ignored.
  void OnPageLifecycleStateChanged(
      const performance_manager::PageNode* page_node) override {}
  // Ignored.
  void OnPageOriginTrialFreezePolicyChanged(
      const performance_manager::PageNode* page_node) override {}
  // Ignored.
  void OnPageIsHoldingWebLockChanged(
      const performance_manager::PageNode* page_node) override {}
  // Ignored.
  void OnPageIsHoldingIndexedDBLockChanged(
      const performance_manager::PageNode* page_node) override {}
  void OnMainFrameUrlChanged(
      const performance_manager::PageNode* page_node) override;
  // Ignored.
  void OnMainFrameDocumentChanged(
      const performance_manager::PageNode* page_node) override {}
  void OnHadFormInteractionChanged(
      const performance_manager::PageNode* page_node) override {}
  void OnTitleUpdated(const performance_manager::PageNode* page_node) override {
  }  // Ignored.
  void OnFaviconUpdated(
      const performance_manager::PageNode* page_node) override;

  // ProcessNodeObserver implementation:
  void OnProcessNodeAdded(
      const performance_manager::ProcessNode* process_node) override;
  void OnProcessLifetimeChange(
      const performance_manager::ProcessNode* process_node) override;
  void OnBeforeProcessNodeRemoved(
      const performance_manager::ProcessNode* process_node) override;
  void OnExpectedTaskQueueingDurationSample(
      const performance_manager::ProcessNode* process_node) override {
  }  // Ignored.
  // Ignored.
  void OnMainThreadTaskLoadIsLow(
      const performance_manager::ProcessNode* process_node) override {}
  // Ignored.
  void OnAllFramesInProcessFrozen(
      const performance_manager::ProcessNode* process_node) override {}
  void OnPriorityChanged(const performance_manager::ProcessNode* process_node,
                         base::TaskPriority previous_value) override {}

  // performance_manager::WorkerNodeObserver implementation:
  void OnWorkerNodeAdded(
      const performance_manager::WorkerNode* worker_node) override;
  void OnBeforeWorkerNodeRemoved(
      const performance_manager::WorkerNode* worker_node) override;
  void OnFinalResponseURLDetermined(
      const performance_manager::WorkerNode* worker_node) override;
  void OnClientFrameAdded(
      const performance_manager::WorkerNode* worker_node,
      const performance_manager::FrameNode* client_frame_node) override;
  void OnBeforeClientFrameRemoved(
      const performance_manager::WorkerNode* worker_node,
      const performance_manager::FrameNode* client_frame_node) override;
  void OnClientWorkerAdded(
      const performance_manager::WorkerNode* worker_node,
      const performance_manager::WorkerNode* client_worker_node) override;
  void OnBeforeClientWorkerRemoved(
      const performance_manager::WorkerNode* worker_node,
      const performance_manager::WorkerNode* client_worker_node) override;

 private:
  // The favicon requests happen on the UI thread. This helper class
  // maintains the state required to do that.
  class FaviconRequestHelper;
  using NodeId = util::IdType64<class NodeIdTag>;

  void AddNode(const performance_manager::Node* node);
  void RemoveNode(const performance_manager::Node* node);
  int64_t GetNodeId(const performance_manager::Node* node);

  FaviconRequestHelper* EnsureFaviconRequestHelper();

  void StartPageFaviconRequest(const performance_manager::PageNode* page_node);
  void StartFrameFaviconRequest(
      const performance_manager::FrameNode* frame_node);

  void SendFrameNotification(const performance_manager::FrameNode* frame,
                             bool created);
  void SendPageNotification(const performance_manager::PageNode* page,
                            bool created);
  void SendProcessNotification(const performance_manager::ProcessNode* process,
                               bool created);
  void SendWorkerNotification(const performance_manager::WorkerNode* worker,
                              bool created);
  void SendDeletionNotification(const performance_manager::Node* node);
  void SendFaviconNotification(
      int64_t serialization_id,
      scoped_refptr<base::RefCountedMemory> bitmap_data);

  static void BindOnPMSequence(
      mojo::PendingReceiver<discards::mojom::GraphDump> receiver,
      performance_manager::Graph* graph);
  static void OnConnectionError(DiscardsGraphDumpImpl* impl);

  performance_manager::Graph* graph_ = nullptr;

  std::unique_ptr<FaviconRequestHelper> favicon_request_helper_;

  // The live nodes and their IDs.
  base::flat_map<const performance_manager::Node*, NodeId> node_ids_;
  base::flat_map<NodeId, const performance_manager::Node*> nodes_by_id_;
  NodeId::Generator node_id_generator_;

  // The current change subscriber to this dumper. This instance is subscribed
  // to every node in |graph_| save for the system node, so long as there is a
  // subscriber.
  mojo::Remote<discards::mojom::GraphChangeStream> change_subscriber_;
  mojo::Receiver<discards::mojom::GraphDump> receiver_{this};

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DiscardsGraphDumpImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DiscardsGraphDumpImpl);
};

#endif  // CHROME_BROWSER_UI_WEBUI_DISCARDS_GRAPH_DUMP_IMPL_H_
