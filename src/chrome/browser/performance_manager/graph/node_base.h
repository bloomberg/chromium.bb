// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_NODE_BASE_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_NODE_BASE_H_

#include <stdint.h>
#include <map>
#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "chrome/browser/performance_manager/graph/graph_impl.h"
#include "chrome/browser/performance_manager/graph/node_type.h"
#include "chrome/browser/performance_manager/graph/properties.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"

namespace performance_manager {

// TODO(chrisha): Remove all of these when GraphObserver is killed.
class GraphObserver;

// NodeBase implements shared functionality among different types of graph
// nodes. A specific type of graph node will derive from this class and can
// override shared functionality when needed.
// All node classes allow construction on one sequence and subsequent use from
// another sequence.
// All methods not documented otherwise are single-threaded.
class NodeBase {
 public:
  // TODO(siggi): Don't store the node type, expose it on a virtual function
  //    instead.
  NodeBase(NodeTypeEnum type, GraphImpl* graph);
  virtual ~NodeBase();

  // May be called on any sequence.
  NodeTypeEnum type() const { return type_; }

  // May be called on any sequence.
  GraphImpl* graph() const { return graph_; }

  // Returns an opaque ID for |node|, unique across all nodes in the same graph,
  // zero for nullptr. This should never be used to look up nodes, only to
  // provide a stable ID for serialization.
  static int64_t GetSerializationId(NodeBase* node);

  // TODO(chrisha): Remove this after observer migration.
  // Implementations of these are provided in the *_node_impl.cc translation
  // units for now.
  void RemoveObserver(GraphObserver* observer);

 protected:
  friend class GraphImpl;

  // Called just before joining |graph_|, a good opportunity to initialize
  // node state.
  virtual void JoinGraph();
  // Called just before leaving |graph_|, a good opportunity to uninitialize
  // node state.
  virtual void LeaveGraph();

  GraphImpl* const graph_;
  const NodeTypeEnum type_;

  // Assigned on first use, immutable from that point forward.
  int64_t serialization_id_ = 0u;

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  DISALLOW_COPY_AND_ASSIGN(NodeBase);
};

// Helper for implementing the common bits of |PublicNodeClass|.
template <class NodeImplClass, class PublicNodeClass>
class PublicNodeImpl : public PublicNodeClass {
 public:
  // Partial implementation of PublicNodeClass:
  Graph* GetGraph() const override {
    return static_cast<const NodeImplClass*>(this)->graph();
  }
  const void* GetIndexingKey() const override {
    // By contract the indexing key is actually a NodeBase pointer. This allows
    // quick and safe casting from a public node type to the corresponding
    // internal node type.
    return static_cast<const NodeBase*>(
        static_cast<const NodeImplClass*>(this));
  }
};

template <class NodeImplClass, class NodeImplObserverClass>
class TypedNodeBase : public NodeBase {
 public:
  using Observer = NodeImplObserverClass;
  using ObserverList =
      typename base::ObserverList<NodeImplObserverClass>::Unchecked;
  using ObservedProperty =
      ObservedPropertyImpl<NodeImplClass, NodeImplObserverClass>;

  explicit TypedNodeBase(GraphImpl* graph)
      : NodeBase(NodeImplClass::Type(), graph) {}

  static const NodeImplClass* FromNodeBase(const NodeBase* node) {
    DCHECK_EQ(node->type(), NodeImplClass::Type());
    return static_cast<const NodeImplClass*>(node);
  }

  static NodeImplClass* FromNodeBase(NodeBase* node) {
    DCHECK(node->type() == NodeImplClass::Type());
    return static_cast<NodeImplClass*>(node);
  }

  void AddObserver(Observer* observer) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    observers_.RemoveObserver(observer);
  }

  const ObserverList& observers() const { return observers_; }

 private:
  ObserverList observers_;

  DISALLOW_COPY_AND_ASSIGN(TypedNodeBase);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_NODE_BASE_H_
