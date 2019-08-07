// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_GRAPH_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_GRAPH_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "chrome/browser/performance_manager/graph/node_attached_data.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_id.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_types.h"

namespace performance_manager {

class NodeBase;
class GraphObserver;
class FrameNodeImpl;
class PageNodeImpl;
class ProcessNodeImpl;
class SystemNodeImpl;

// The Graph represents a graph of the coordination units
// representing a single system. It vends out new instances of coordination
// units and indexes them by ID. It also fires the creation and pre-destruction
// notifications for all coordination units.
class Graph {
 public:
  using CUIDMap =
      std::unordered_map<resource_coordinator::CoordinationUnitID, NodeBase*>;

  Graph();
  ~Graph();

  void set_ukm_recorder(ukm::UkmRecorder* ukm_recorder) {
    ukm_recorder_ = ukm_recorder;
  }
  ukm::UkmRecorder* ukm_recorder() const { return ukm_recorder_; }

  // Register |observer| on the graph.
  void RegisterObserver(GraphObserver* observer);

  // Unregister |observer| from observing graph changes. Note that this does not
  // unregister |observer| from any nodes it's subscribed to.
  void UnregisterObserver(GraphObserver* observer);

  SystemNodeImpl* FindOrCreateSystemNode();
  std::vector<ProcessNodeImpl*> GetAllProcessNodes();
  std::vector<FrameNodeImpl*> GetAllFrameNodes();
  std::vector<PageNodeImpl*> GetAllPageNodes();
  const CUIDMap& nodes() { return nodes_; }

  // Retrieves the process CU with PID |pid|, if any.
  ProcessNodeImpl* GetProcessNodeByPid(base::ProcessId pid);
  NodeBase* GetNodeByID(const resource_coordinator::CoordinationUnitID cu_id);

  std::vector<GraphObserver*>& observers_for_testing() { return observers_; }

  // Management functions for node owners, any node added to the graph must be
  // removed from the graph before it's deleted.
  void AddNewNode(NodeBase* new_node);
  void RemoveNode(NodeBase* node);

  // A |key| of nullptr counts all instances associated with the |node|. A
  // |node| of null counts all instances associated with the |key|. If both are
  // null then the entire map size is provided.
  size_t GetNodeAttachedDataCountForTesting(NodeBase* node,
                                            const void* key) const;

 private:
  using ProcessByPidMap = std::unordered_map<base::ProcessId, ProcessNodeImpl*>;

  void OnNodeAdded(NodeBase* node);
  void OnBeforeNodeRemoved(NodeBase* node);

  // Process PID map for use by ProcessNodeImpl.
  friend class ProcessNodeImpl;
  void BeforeProcessPidChange(ProcessNodeImpl* process,
                              base::ProcessId new_pid);

  template <typename CUType>
  std::vector<CUType*> GetAllNodesOfType();

  std::unique_ptr<SystemNodeImpl> system_node_;
  CUIDMap nodes_;
  ProcessByPidMap processes_by_pid_;
  std::vector<GraphObserver*> observers_;
  ukm::UkmRecorder* ukm_recorder_ = nullptr;

  // User data storage for the graph.
  friend class NodeAttachedData;
  using NodeAttachedDataKey = std::pair<const NodeBase*, const void*>;
  using NodeAttachedDataMap =
      std::map<NodeAttachedDataKey, std::unique_ptr<NodeAttachedData>>;
  NodeAttachedDataMap node_attached_data_map_;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(Graph);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_GRAPH_H_
