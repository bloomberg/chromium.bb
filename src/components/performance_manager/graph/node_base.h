// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_NODE_BASE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_NODE_BASE_H_

#include <stdint.h>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/node_type.h"
#include "components/performance_manager/graph/properties.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace performance_manager {

class Node;

// NodeBase implements shared functionality among different types of graph
// nodes. A specific type of graph node will derive from this class and can
// override shared functionality when needed.
// All node classes allow construction on one sequence and subsequent use from
// another sequence.
// All methods not documented otherwise are single-threaded.
class NodeBase {
 public:
  // Used as a unique key to safely allow downcasting from a public node type
  // to NodeBase via "GetImplType" and "GetImpl". The implementations are
  // provided by PublicNodeImpl below.
  static const uintptr_t kNodeBaseType;

  // TODO(siggi): Don't store the node type, expose it on a virtual function
  //    instead.
  explicit NodeBase(NodeTypeEnum type);
  virtual ~NodeBase();

  // May be called on any sequence.
  NodeTypeEnum type() const { return type_; }

  // Returns the graph that contains this node. Only valid after JoinGraph() and
  // before LeaveGraph().
  GraphImpl* graph() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(graph_);
    return graph_;
  }

  // Helper functions for casting from a node type to its underlying NodeBase.
  // This CHECKs that the cast is valid. These functions work happily with
  // public and private node class inputs.
  static const NodeBase* FromNode(const Node* node);
  static NodeBase* FromNode(Node* node);

  // For converting from NodeBase to Node. This is implemented by
  // TypedNodeBase.
  virtual const Node* ToNode() const = 0;

 protected:
  friend class GraphImpl;

  // Helper function for TypedNodeBase to access the list of typed observers
  // stored in the graph.
  template <typename Observer>
  static const std::vector<Observer*>& GetObservers(const GraphImpl* graph) {
    return graph->GetObservers<Observer>();
  }

  // Joins the |graph|. Assigns |graph_| and invokes OnJoiningGraph() to allow
  // subclasses to initialize.
  void JoinGraph(GraphImpl* graph);

  // Leaves the graph that this node is a part of. Invokes
  // OnBeforeLeavingGraph() to allow subclasses to uninitialize then clears
  // |graph_|.
  void LeaveGraph();

  // Called as this node is joining |graph_|, a good opportunity to initialize
  // node state.
  virtual void OnJoiningGraph();
  // Called just before leaving |graph_|, a good opportunity to uninitialize
  // node state.
  virtual void OnBeforeLeavingGraph();

  const NodeTypeEnum type_;

  // Assigned when JoinGraph() is called, up until LeaveGraph() is called, where
  // it is reset to null.
  GraphImpl* graph_ = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  DISALLOW_COPY_AND_ASSIGN(NodeBase);
};

// Helper for implementing the Node parent of a PublicNodeClass.
template <class NodeImplClass, class PublicNodeClass>
class PublicNodeImpl : public PublicNodeClass {
 public:
  // Node implementation:
  Graph* GetGraph() const override {
    return static_cast<const NodeImplClass*>(this)->graph();
  }
  uintptr_t GetImplType() const override { return NodeBase::kNodeBaseType; }
  const void* GetImpl() const override {
    // This exposes NodeBase, so that we can complete the triangle of casting
    // between all views of a node: NodeBase, FooNodeImpl, and FooNode.
    return static_cast<const NodeBase*>(
        static_cast<const NodeImplClass*>(this));
  }
};

template <class NodeImplClass, class NodeClass, class NodeObserverClass>
class TypedNodeBase : public NodeBase {
 public:
  using ObservedProperty =
      ObservedPropertyImpl<NodeImplClass, NodeClass, NodeObserverClass>;

  TypedNodeBase() : NodeBase(NodeImplClass::Type()) {}

  // Helper functions for casting from NodeBase to a concrete node type. This
  // CHECKs that the cast is valid.
  static const NodeImplClass* FromNodeBase(const NodeBase* node) {
    CHECK_EQ(NodeImplClass::Type(), node->type());
    return static_cast<const NodeImplClass*>(node);
  }
  static NodeImplClass* FromNodeBase(NodeBase* node) {
    CHECK_EQ(NodeImplClass::Type(), node->type());
    return static_cast<NodeImplClass*>(node);
  }

  // Helper function for casting from a public node type to the private impl.
  // This also casts away const correctness, as it is intended to be used by
  // impl code that uses the public observer interface for a node type, where
  // all notifications are delivered with a const node pointer. This CHECKs that
  // the cast is valid.
  static NodeImplClass* FromNode(const NodeClass* node) {
    NodeBase* node_base = const_cast<NodeBase*>(NodeBase::FromNode(node));
    return FromNodeBase(node_base);
  }

  // Convenience accessor to the per-node-class list of observers that is stored
  // in the graph.
  const std::vector<NodeObserverClass*>& GetObservers() const {
    // Mediate through NodeBase, as it's the class that is friended by the
    // GraphImpl in order to provide access.
    return NodeBase::GetObservers<NodeObserverClass>(graph());
  }

  const Node* ToNode() const override {
    return static_cast<const NodeImplClass*>(this);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TypedNodeBase);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_NODE_BASE_H_
