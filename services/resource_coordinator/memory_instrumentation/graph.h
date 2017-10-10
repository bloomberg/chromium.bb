// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_GRAPH_H_
#define SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_GRAPH_H_

#include <forward_list>
#include <map>
#include <memory>

#include "base/gtest_prod_util.h"
#include "base/process/process_handle.h"
#include "base/trace_event/memory_allocator_dump_guid.h"

namespace memory_instrumentation {

// Contains processed dump graphs for each process and in the global space.
// This class is also the arena which owns the nodes of the graph.
class GlobalDumpGraph {
 public:
  class Node;
  class Edge;

  // Graph of dumps either associated with a process or with
  // the shared space.
  class Process {
   public:
    explicit Process(GlobalDumpGraph* global_graph);
    ~Process();

    // Creates a node in the dump graph which is associated with the
    // given |guid| and |path| and returns it.
    GlobalDumpGraph::Node* CreateNode(
        base::trace_event::MemoryAllocatorDumpGuid guid,
        base::StringPiece path);

    // Returns the node in the graph at the given |path| or nullptr
    // if no such node exists in the provided |graph|.
    GlobalDumpGraph::Node* FindNode(base::StringPiece path);

   private:
    GlobalDumpGraph* const global_graph_;
    GlobalDumpGraph::Node* root_;

    DISALLOW_COPY_AND_ASSIGN(Process);
  };

  // A single node in the graph of allocator dumps associated with a
  // certain path and containing the entries for this path.
  class Node {
   public:
    // Auxilary data (a scalar number or a string) about this node each
    // associated with a key.
    struct Entry {
      enum Type {
        kUInt64,
        kString,
      };

      // The units of the entry if the entry is a scalar. The scalar
      // refers to either a number of objects or a size in bytes.
      enum ScalarUnits {
        kObjects,
        kBytes,
      };

      // Creates the entry with the appropriate type.
      Entry(ScalarUnits units, uint64_t value);
      Entry(std::string value);

      const Type type;
      const ScalarUnits units;

      // The value of the entry if this entry has a string type.
      const std::string value_string;

      // The value of the entry if this entry has a integer type.
      const uint64_t value_uint64;
    };

    explicit Node(GlobalDumpGraph::Process* dump_graph);
    ~Node();

    // Gets the direct child of a node for the given |subpath|.
    Node* GetChild(base::StringPiece name);

    // Inserts the given |node| as a child of the current node
    // with the given |subpath| as the key.
    void InsertChild(base::StringPiece name, Node* node);

    // Adds an entry for this dump node with the given |name|, |units| and
    // type.
    void AddEntry(std::string name, Entry::ScalarUnits units, uint64_t value);
    void AddEntry(std::string name, std::string value);

    // Adds an edge which indicates that this node is owned by
    // another node.
    void AddOwnedByEdge(Edge* edge);

    // Sets the edge indicates that this node owns another node.
    void SetOwnsEdge(Edge* edge);

    GlobalDumpGraph::Edge* owns_edge() const { return owns_edge_; }
    const std::vector<GlobalDumpGraph::Edge*>& owned_by_edges() const {
      return owned_by_edges_;
    }
    const GlobalDumpGraph::Process* dump_graph() const { return dump_graph_; }
    const std::map<std::string, Entry>& entries() const { return entries_; }

   private:
    GlobalDumpGraph::Process* const dump_graph_;
    std::map<std::string, Entry> entries_;
    std::map<std::string, Node*> children_;

    GlobalDumpGraph::Edge* owns_edge_;
    std::vector<GlobalDumpGraph::Edge*> owned_by_edges_;

    DISALLOW_COPY_AND_ASSIGN(Node);
  };

  // An edge in the dump graph which indicates ownership between the
  // source and target nodes.
  class Edge {
   public:
    Edge(GlobalDumpGraph::Node* source,
         GlobalDumpGraph::Node* target,
         int priority);

    GlobalDumpGraph::Node* source() const { return source_; }
    GlobalDumpGraph::Node* target() const { return target_; }
    int priority() const { return priority_; }

   private:
    GlobalDumpGraph::Node* const source_;
    GlobalDumpGraph::Node* const target_;
    const int priority_;
  };

  using ProcessDumpGraphMap =
      std::map<base::ProcessId, std::unique_ptr<GlobalDumpGraph::Process>>;
  using GuidNodeMap =
      std::map<base::trace_event::MemoryAllocatorDumpGuid, Node*>;

  GlobalDumpGraph();
  ~GlobalDumpGraph();

  // Creates a container for all the dump graphs for the process given
  // by the given |process_id|.
  GlobalDumpGraph::Process* CreateGraphForProcess(base::ProcessId process_id);

  // Adds an edge in the dump graph with the given source and target nodes
  // and edge priority.
  void AddNodeOwnershipEdge(Node* owner, Node* owned, int priority);

  const GuidNodeMap& nodes_by_guid() const { return nodes_by_guid_; }
  GlobalDumpGraph::Process* shared_memory_graph() const {
    return shared_memory_graph_.get();
  }
  const ProcessDumpGraphMap& process_dump_graphs() const {
    return process_dump_graphs_;
  }
  const std::forward_list<Edge> edges() const { return all_edges_; }

 private:
  // Creates a node in the arena which is associated with the given
  // |dump_graph|.
  Node* CreateNode(GlobalDumpGraph::Process* dump_graph);

  std::forward_list<Node> all_nodes_;
  std::forward_list<Edge> all_edges_;
  GuidNodeMap nodes_by_guid_;
  std::unique_ptr<GlobalDumpGraph::Process> shared_memory_graph_;
  ProcessDumpGraphMap process_dump_graphs_;

  DISALLOW_COPY_AND_ASSIGN(GlobalDumpGraph);
};

}  // namespace memory_instrumentation
#endif