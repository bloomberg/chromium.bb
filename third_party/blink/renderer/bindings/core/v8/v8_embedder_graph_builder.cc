// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_embedder_graph_builder.h"

#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable_visitor.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"

namespace blink {

namespace {

using Traceable = const void*;
using Graph = v8::EmbedderGraph;

// Information about whether a node is attached to the main DOM tree
// or not. It is computed as follows:
// 1) A Document with IsContextDestroyed() = true is detached.
// 2) A Document with IsContextDestroyed() = false is attached.
// 3) A Node that is not connected to any Document is detached.
// 4) A Node that is connected to a detached Document is detached.
// 5) A Node that is connected to an attached Document is attached.
// 6) A ScriptWrappable that is reachable from an attached Node is
//    attached.
// 7) A ScriptWrappable that is reachable from a detached Node is
//    detached.
// 8) A ScriptWrappable that is not reachable from any Node is
//    considered (conservatively) as attached.
// The unknown state applies to ScriptWrappables during graph
// traversal when we don't have reachability information yet.
enum class DomTreeState { kAttached, kDetached, kUnknown };

DomTreeState DomTreeStateFromWrapper(v8::Isolate* isolate,
                                     uint16_t class_id,
                                     v8::Local<v8::Object> v8_value) {
  if (class_id != WrapperTypeInfo::kNodeClassId)
    return DomTreeState::kUnknown;
  Node* node = V8Node::ToImpl(v8_value);
  Node* root = V8GCController::OpaqueRootForGC(isolate, node);
  if (root->isConnected() &&
      !node->GetDocument().MasterDocument().IsContextDestroyed()) {
    return DomTreeState::kAttached;
  }
  return DomTreeState::kDetached;
}

class EmbedderNode : public Graph::Node {
 public:
  EmbedderNode(const char* name,
               Graph::Node* wrapper,
               DomTreeState dom_tree_state)
      : name_(name), wrapper_(wrapper), dom_tree_state_(dom_tree_state) {}

  DomTreeState GetDomTreeState() { return dom_tree_state_; }
  void UpdateDomTreeState(DomTreeState parent_dom_tree_state) {
    // If the child's state is unknown, then take the parent's state.
    // If the parent is attached, then the child is also attached.
    if (dom_tree_state_ == DomTreeState::kUnknown ||
        parent_dom_tree_state == DomTreeState::kAttached) {
      dom_tree_state_ = parent_dom_tree_state;
    }
  }
  // Graph::Node overrides.
  const char* Name() override { return name_; }
  const char* NamePrefix() override {
    return dom_tree_state_ == DomTreeState::kDetached ? "Detached" : nullptr;
  }
  size_t SizeInBytes() override { return 0; }
  Graph::Node* WrapperNode() override { return wrapper_; }

 private:
  const char* name_;
  Graph::Node* wrapper_;
  DomTreeState dom_tree_state_;
};

class EmbedderRootNode : public EmbedderNode {
 public:
  explicit EmbedderRootNode(const char* name)
      : EmbedderNode(name, nullptr, DomTreeState::kUnknown) {}
  // Graph::Node override.
  bool IsRootNode() override { return true; }
};

class NodeBuilder final {
 public:
  explicit NodeBuilder(Graph* graph) : graph_(graph) {}

  Graph::Node* GraphNode(const v8::Local<v8::Value>&);
  EmbedderNode* GraphNode(Traceable,
                          const char* name,
                          Graph::Node* wrapper,
                          DomTreeState);
  bool Contains(Traceable traceable) const {
    return graph_nodes_.Contains(traceable);
  }

 private:
  Graph* const graph_;
  HashMap<Traceable, EmbedderNode*> graph_nodes_;
};

v8::EmbedderGraph::Node* NodeBuilder::GraphNode(
    const v8::Local<v8::Value>& value) {
  return graph_->V8Node(value);
}

EmbedderNode* NodeBuilder::GraphNode(Traceable traceable,
                                     const char* name,
                                     v8::EmbedderGraph::Node* wrapper,
                                     DomTreeState dom_tree_state) {
  auto iter = graph_nodes_.find(traceable);
  if (iter != graph_nodes_.end()) {
    iter->value->UpdateDomTreeState(dom_tree_state);
    return iter->value;
  }
  // Ownership of the new node is transferred to the graph_.
  // graph_node_.at(tracable) is valid for all BuildEmbedderGraph execution.
  auto* raw_node = new EmbedderNode(name, wrapper, dom_tree_state);
  EmbedderNode* node = static_cast<EmbedderNode*>(
      graph_->AddNode(std::unique_ptr<Graph::Node>(raw_node)));
  graph_nodes_.insert(traceable, node);
  return node;
}

}  // namespace

class V8EmbedderGraphBuilder
    : public Visitor,
      public v8::PersistentHandleVisitor,
      public v8::EmbedderHeapTracer::TracedGlobalHandleVisitor {
 public:
  V8EmbedderGraphBuilder(v8::Isolate*, Graph*, NodeBuilder*);

  void BuildEmbedderGraph();

  // v8::PersistentHandleVisitor override.
  void VisitPersistentHandle(v8::Persistent<v8::Value>*,
                             uint16_t class_id) override;

  // v8::EmbedderHeapTracer::TracedGlobalHandleVisitor override.
  void VisitTracedGlobalHandle(
      const v8::TracedGlobal<v8::Value>& value) override;

  // Visitor overrides.
  void Visit(const TraceWrapperV8Reference<v8::Value>&) final;
  void VisitWithWrappers(void*, TraceDescriptor) final;
  void VisitBackingStoreStrongly(void* object,
                                 void** object_slot,
                                 TraceDescriptor desc) final;

  // Unused Visitor overrides.
  void Visit(void* object, TraceDescriptor desc) final {
    // TODO(mlippautz): Implement for unified heap snapshots.
  }
  void VisitWeak(void* object,
                 void** object_slot,
                 TraceDescriptor desc,
                 WeakCallback callback) final {}
  void VisitBackingStoreWeakly(void*,
                               void**,
                               TraceDescriptor,
                               WeakCallback,
                               void*) final {}
  void VisitBackingStoreOnly(void*, void**) final {}
  void RegisterBackingStoreCallback(void**, MovingObjectCallback, void*) final {
  }
  void RegisterWeakCallback(void*, WeakCallback) final {}

 private:
  class ParentScope {
    STACK_ALLOCATED();

   public:
    ParentScope(V8EmbedderGraphBuilder* visitor, EmbedderNode* parent)
        : visitor_(visitor) {
      DCHECK_EQ(visitor->current_parent_, nullptr);
      visitor->current_parent_ = parent;
    }
    ~ParentScope() { visitor_->current_parent_ = nullptr; }

   private:
    V8EmbedderGraphBuilder* const visitor_;
  };

  struct WorklistItem {
    EmbedderNode* node;
    Traceable traceable;
    TraceCallback trace_callback;
  };

  void VisitPersistentHandleInternal(v8::Local<v8::Object>, uint16_t);

  WorklistItem ToWorklistItem(EmbedderNode*, const TraceDescriptor&) const;

  void VisitPendingActivities();
  void VisitTransitiveClosure();

  // Push the item to the default worklist if item.traceable was not
  // already visited.
  void PushToWorklist(WorklistItem);

  v8::Isolate* const isolate_;
  Graph* const graph_;
  NodeBuilder* const node_builder_;

  EmbedderNode* current_parent_ = nullptr;

  HashSet<Traceable> visited_;
  // The default worklist that is used to visit transitive closure.
  Deque<WorklistItem> worklist_;
  // The worklist that collects detached Nodes during persistent handle
  // iteration.
  Deque<WorklistItem> detached_worklist_;
  // The worklist that collects ScriptWrappables with unknown information
  // about attached/detached state during persistent handle iteration.
  Deque<WorklistItem> unknown_worklist_;
};

V8EmbedderGraphBuilder::V8EmbedderGraphBuilder(v8::Isolate* isolate,
                                               Graph* graph,
                                               NodeBuilder* node_builder)
    : Visitor(ThreadState::Current()),
      isolate_(isolate),
      graph_(graph),
      node_builder_(node_builder) {
  CHECK(isolate);
  CHECK(graph);
  CHECK_EQ(isolate, ThreadState::Current()->GetIsolate());
}

void V8EmbedderGraphBuilder::BuildEmbedderGraph() {
  isolate_->VisitHandlesWithClassIds(this);
  v8::EmbedderHeapTracer* tracer =
      V8PerIsolateData::From(isolate_)->GetEmbedderHeapTracer();
  if (tracer)
    tracer->IterateTracedGlobalHandles(this);
// At this point we collected ScriptWrappables in three groups:
// attached, detached, and unknown.
#if DCHECK_IS_ON()
  for (const WorklistItem& item : worklist_) {
    DCHECK_EQ(DomTreeState::kAttached, item.node->GetDomTreeState());
  }
  for (const WorklistItem& item : detached_worklist_) {
    DCHECK_EQ(DomTreeState::kDetached, item.node->GetDomTreeState());
  }
  for (const WorklistItem& item : unknown_worklist_) {
    DCHECK_EQ(DomTreeState::kUnknown, item.node->GetDomTreeState());
  }
#endif
  // We need to propagate attached/detached information to ScriptWrappables
  // with the unknown state. The information propagates from a parent to
  // a child as follows:
  // - if the parent is attached, then the child is considered attached.
  // - if the parent is detached and the child is unknown, then the child is
  //   considered detached.
  // - if the parent is unknown, then the state of the child does not change.
  //
  // We need to organize DOM traversal in three stages to ensure correct
  // propagation:
  // 1) Traverse from the attached nodes. All nodes discovered in this stage
  //    will be marked as kAttached.
  // 2) Traverse from the detached nodes. All nodes discovered in this stage
  //    will be marked as kDetached if they are not already marked as kAttached.
  // 3) Traverse from the unknown nodes. This is needed only for edge recording.
  // Stage 1: find transitive closure of the attached nodes.
  VisitTransitiveClosure();
  // Stage 2: find transitive closure of the detached nodes.
  while (!detached_worklist_.empty()) {
    auto item = detached_worklist_.back();
    detached_worklist_.pop_back();
    PushToWorklist(item);
  }
  VisitTransitiveClosure();
  // Stage 3: find transitive closure of the unknown nodes.
  // Nodes reachable only via pending activities are treated as unknown.
  VisitPendingActivities();
  while (!unknown_worklist_.empty()) {
    auto item = unknown_worklist_.back();
    unknown_worklist_.pop_back();
    PushToWorklist(item);
  }
  VisitTransitiveClosure();
  DCHECK(worklist_.empty());
  DCHECK(detached_worklist_.empty());
  DCHECK(unknown_worklist_.empty());
}

void V8EmbedderGraphBuilder::VisitPersistentHandleInternal(
    v8::Local<v8::Object> v8_value,
    uint16_t class_id) {
  ScriptWrappable* traceable = ToScriptWrappable(v8_value);
  if (!traceable)
    return;
  Graph::Node* wrapper = node_builder_->GraphNode(v8_value);
  DomTreeState dom_tree_state =
      DomTreeStateFromWrapper(isolate_, class_id, v8_value);
  EmbedderNode* graph_node = node_builder_->GraphNode(
      traceable, traceable->NameInHeapSnapshot(), wrapper, dom_tree_state);
  const TraceDescriptor& descriptor =
      TraceDescriptorFor<ScriptWrappable>(traceable);
  WorklistItem item = ToWorklistItem(graph_node, descriptor);
  switch (graph_node->GetDomTreeState()) {
    case DomTreeState::kAttached:
      PushToWorklist(item);
      break;
    case DomTreeState::kDetached:
      detached_worklist_.push_back(item);
      break;
    case DomTreeState::kUnknown:
      unknown_worklist_.push_back(item);
      break;
  }
}

void V8EmbedderGraphBuilder::VisitTracedGlobalHandle(
    const v8::TracedGlobal<v8::Value>& value) {
  const uint16_t class_id = value.WrapperClassId();
  if (class_id != WrapperTypeInfo::kNodeClassId &&
      class_id != WrapperTypeInfo::kObjectClassId)
    return;
  VisitPersistentHandleInternal(value.As<v8::Object>().Get(isolate_), class_id);
}

void V8EmbedderGraphBuilder::VisitPersistentHandle(
    v8::Persistent<v8::Value>* value,
    uint16_t class_id) {
  if (class_id != WrapperTypeInfo::kNodeClassId &&
      class_id != WrapperTypeInfo::kObjectClassId)
    return;
  v8::Local<v8::Object> v8_value = v8::Local<v8::Object>::New(
      isolate_, v8::Persistent<v8::Object>::Cast(*value));
  VisitPersistentHandleInternal(v8_value, class_id);
}

void V8EmbedderGraphBuilder::Visit(
    const TraceWrapperV8Reference<v8::Value>& traced_wrapper) {
  // Add an edge from the current parent to the V8 object.
  v8::Local<v8::Value> v8_value = traced_wrapper.NewLocal(isolate_);
  if (!v8_value.IsEmpty()) {
    graph_->AddEdge(current_parent_, node_builder_->GraphNode(v8_value));
  }
}

void V8EmbedderGraphBuilder::VisitWithWrappers(
    void* object,
    TraceDescriptor wrapper_descriptor) {
  // Add an edge from the current parent to this object.
  // Also push the object to the worklist in order to process its members.
  const void* traceable = wrapper_descriptor.base_object_payload;
  const char* name =
      GCInfoTable::Get()
          .GCInfoFromIndex(
              HeapObjectHeader::FromPayload(traceable)->GcInfoIndex())
          ->name(traceable)
          .value;
  EmbedderNode* graph_node = node_builder_->GraphNode(
      traceable, name, nullptr, current_parent_->GetDomTreeState());
  graph_->AddEdge(current_parent_, graph_node);
  PushToWorklist(ToWorklistItem(graph_node, wrapper_descriptor));
}

void V8EmbedderGraphBuilder::VisitBackingStoreStrongly(void* object,
                                                       void** object_slot,
                                                       TraceDescriptor desc) {
  if (!object)
    return;
  desc.callback(this, desc.base_object_payload);
}

void V8EmbedderGraphBuilder::VisitPendingActivities() {
  // Ownership of the new node is transferred to the graph_.
  EmbedderNode* root =
      static_cast<EmbedderNode*>(graph_->AddNode(std::unique_ptr<Graph::Node>(
          new EmbedderRootNode("Pending activities"))));
  ParentScope parent(this, root);
  ActiveScriptWrappableBase::TraceActiveScriptWrappables(isolate_, this);
}

V8EmbedderGraphBuilder::WorklistItem V8EmbedderGraphBuilder::ToWorklistItem(
    EmbedderNode* node,
    const TraceDescriptor& descriptor) const {
  return {node, descriptor.base_object_payload, descriptor.callback};
}

void V8EmbedderGraphBuilder::PushToWorklist(WorklistItem item) {
  if (!visited_.Contains(item.traceable)) {
    visited_.insert(item.traceable);
    worklist_.push_back(item);
  }
}

void V8EmbedderGraphBuilder::VisitTransitiveClosure() {
  // Depth-first search.
  while (!worklist_.empty()) {
    auto item = worklist_.back();
    worklist_.pop_back();
    ParentScope parent(this, item.node);
    item.trace_callback(this, const_cast<void*>(item.traceable));
  }
}

void EmbedderGraphBuilder::BuildEmbedderGraphCallback(v8::Isolate* isolate,
                                                      v8::EmbedderGraph* graph,
                                                      void*) {
  NodeBuilder node_builder(graph);
  V8EmbedderGraphBuilder builder(isolate, graph, &node_builder);
  builder.BuildEmbedderGraph();
}

}  // namespace blink
