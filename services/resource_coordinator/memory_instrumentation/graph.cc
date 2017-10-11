// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/memory_instrumentation/graph.h"

#include "base/strings/string_tokenizer.h"

namespace memory_instrumentation {

namespace {

using base::trace_event::MemoryAllocatorDumpGuid;
using Edge = GlobalDumpGraph::Edge;
using Process = GlobalDumpGraph::Process;
using Node = GlobalDumpGraph::Node;

}  // namespace

GlobalDumpGraph::GlobalDumpGraph()
    : shared_memory_graph_(std::make_unique<Process>(this)) {}
GlobalDumpGraph::~GlobalDumpGraph() {}

Process* GlobalDumpGraph::CreateGraphForProcess(base::ProcessId process_id) {
  auto id_to_dump_iterator =
      process_dump_graphs_.emplace(process_id, std::make_unique<Process>(this));
  DCHECK(id_to_dump_iterator.second);  // Check for duplicate pids
  return id_to_dump_iterator.first->second.get();
}

void GlobalDumpGraph::AddNodeOwnershipEdge(Node* owner,
                                           Node* owned,
                                           int importance) {
  // The owner node should not already be associated with an edge.
  DCHECK(!owner->owns_edge());

  all_edges_.emplace_front(owner, owned, importance);
  Edge* edge = &*all_edges_.begin();
  owner->SetOwnsEdge(edge);
  owned->AddOwnedByEdge(edge);
}

Node* GlobalDumpGraph::CreateNode(Process* process_graph, Node* parent) {
  all_nodes_.emplace_front(process_graph, parent);
  return &*all_nodes_.begin();
}

Process::Process(GlobalDumpGraph* global_graph)
    : global_graph_(global_graph),
      root_(global_graph->CreateNode(this, nullptr)) {}
Process::~Process() {}

Node* Process::CreateNode(MemoryAllocatorDumpGuid guid,
                          base::StringPiece path) {
  std::string path_string = path.as_string();
  base::StringTokenizer tokenizer(path_string, "/");

  // Perform a tree traversal, creating the nodes if they do not
  // already exist on the path to the child.
  Node* current = root_;
  while (tokenizer.GetNext()) {
    base::StringPiece key = tokenizer.token_piece();
    Node* parent = current;
    current = current->GetChild(key);
    if (!current) {
      current = global_graph_->CreateNode(this, parent);
      parent->InsertChild(key, current);
    }
  }

  // Add to the global guid map as well.
  global_graph_->nodes_by_guid_.emplace(guid, current);
  return current;
}

Node* Process::FindNode(base::StringPiece path) {
  std::string path_string = path.as_string();
  base::StringTokenizer tokenizer(path_string, "/");
  Node* current = root_;
  while (tokenizer.GetNext()) {
    base::StringPiece key = tokenizer.token_piece();
    current = current->GetChild(key);
    if (!current)
      return nullptr;
  }
  return current;
}

Node::Node(Process* dump_graph, Node* parent)
    : dump_graph_(dump_graph), parent_(parent), owns_edge_(nullptr) {}
Node::~Node() {}

Node* Node::GetChild(base::StringPiece name) {
  DCHECK(!name.empty());
  DCHECK_EQ(std::string::npos, name.find('/'));

  auto child = children_.find(name.as_string());
  return child == children_.end() ? nullptr : child->second;
}

void Node::InsertChild(base::StringPiece name, Node* node) {
  DCHECK(!name.empty());
  DCHECK_EQ(std::string::npos, name.find('/'));

  children_.emplace(name.as_string(), node);
}

void Node::AddOwnedByEdge(Edge* edge) {
  owned_by_edges_.push_back(edge);
}

void Node::SetOwnsEdge(Edge* owns_edge) {
  owns_edge_ = owns_edge;
}

void Node::AddEntry(std::string name,
                    Node::Entry::ScalarUnits units,
                    uint64_t value) {
  entries_.emplace(name, Node::Entry(units, value));
}

void Node::AddEntry(std::string name, std::string value) {
  entries_.emplace(name, Node::Entry(value));
}

Node::Entry::Entry(Entry::ScalarUnits units, uint64_t value)
    : type(Node::Entry::Type::kUInt64), units(units), value_uint64(value) {}

Node::Entry::Entry(std::string value)
    : type(Node::Entry::Type::kString),
      units(Node::Entry::ScalarUnits::kObjects),
      value_string(value),
      value_uint64(0) {}

Edge::Edge(Node* source, Node* target, int priority)
    : source_(source), target_(target), priority_(priority) {}

}  // namespace memory_instrumentation