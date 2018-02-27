// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8EmbedderGraphBuilder.h"

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "bindings/core/v8/V8Node.h"
#include "platform/bindings/DOMWrapperMap.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/ScriptWrappableVisitor.h"
#include "platform/bindings/WrapperTypeInfo.h"

namespace blink {

V8EmbedderGraphBuilder::V8EmbedderGraphBuilder(v8::Isolate* isolate,
                                               Graph* graph)
    : isolate_(isolate), current_parent_(nullptr), graph_(graph) {}

void V8EmbedderGraphBuilder::BuildEmbedderGraphCallback(
    v8::Isolate* isolate,
    v8::EmbedderGraph* graph) {
  V8EmbedderGraphBuilder builder(isolate, graph);
  builder.BuildEmbedderGraph();
}

void V8EmbedderGraphBuilder::BuildEmbedderGraph() {
  isolate_->VisitHandlesWithClassIds(this);
  VisitPendingActivities();
  VisitTransitiveClosure();
}

void V8EmbedderGraphBuilder::VisitPersistentHandle(
    v8::Persistent<v8::Value>* value,
    uint16_t class_id) {
  if (class_id != WrapperTypeInfo::kNodeClassId &&
      class_id != WrapperTypeInfo::kObjectClassId)
    return;
  v8::Local<v8::Object> v8_value = v8::Local<v8::Object>::New(
      isolate_, v8::Persistent<v8::Object>::Cast(*value));
  ScriptWrappable* traceable = ToScriptWrappable(v8_value);
  if (traceable) {
    // Add v8_value => traceable edge.
    Graph::Node* graph_node =
        GraphNode(traceable, traceable->NameInHeapSnapshot());
    graph_->AddEdge(GraphNode(v8_value), graph_node);
    // Visit traceable members. This will also add traceable => v8_value edge.
    ParentScope parent(this, graph_node);
    traceable->TraceWrappers(this);
  }
}

void V8EmbedderGraphBuilder::Visit(
    const TraceWrapperV8Reference<v8::Value>& traced_wrapper) const {
  const v8::PersistentBase<v8::Value>* value = &traced_wrapper.Get();
  // Add an edge from the current parent to the V8 object.
  v8::Local<v8::Value> v8_value = v8::Local<v8::Value>::New(isolate_, *value);
  if (!v8_value.IsEmpty()) {
    graph_->AddEdge(current_parent_, GraphNode(v8_value));
  }
}

void V8EmbedderGraphBuilder::Visit(
    const WrapperDescriptor& wrapper_descriptor) const {
  // Add an edge from the current parent to this object.
  // Also push the object to the worklist in order to process its members.
  const void* traceable = wrapper_descriptor.traceable;
  Graph::Node* graph_node =
      GraphNode(traceable, wrapper_descriptor.name_callback(traceable));
  graph_->AddEdge(current_parent_, graph_node);
  if (!visited_.Contains(traceable)) {
    visited_.insert(traceable);
    worklist_.push_back(ToWorklistItem(graph_node, wrapper_descriptor));
  }
}

void V8EmbedderGraphBuilder::Visit(DOMWrapperMap<ScriptWrappable>* wrapper_map,
                                   const ScriptWrappable* key) const {
  // Add an edge from the current parent to the V8 object.
  v8::Local<v8::Value> v8_value =
      wrapper_map->NewLocal(isolate_, const_cast<ScriptWrappable*>(key));
  if (!v8_value.IsEmpty())
    graph_->AddEdge(current_parent_, GraphNode(v8_value));
}

v8::EmbedderGraph::Node* V8EmbedderGraphBuilder::GraphNode(
    const v8::Local<v8::Value>& value) const {
  return graph_->V8Node(value);
}

v8::EmbedderGraph::Node* V8EmbedderGraphBuilder::GraphNode(
    Traceable traceable,
    const char* name) const {
  auto iter = graph_node_.find(traceable);
  if (iter != graph_node_.end())
    return iter->value;
  // Ownership of the new node is transferred to the graph_.
  // graph_node_.at(tracable) is valid for all BuildEmbedderGraph execution.
  auto node =
      graph_->AddNode(std::unique_ptr<Graph::Node>(new EmbedderNode(name)));
  graph_node_.insert(traceable, node);
  return node;
}

void V8EmbedderGraphBuilder::VisitPendingActivities() {
  // Ownership of the new node is transferred to the graph_.
  Graph::Node* root = graph_->AddNode(
      std::unique_ptr<Graph::Node>(new EmbedderRootNode("Pending activities")));
  ParentScope parent(this, root);
  ActiveScriptWrappableBase::TraceActiveScriptWrappables(isolate_, this);
}

V8EmbedderGraphBuilder::WorklistItem V8EmbedderGraphBuilder::ToWorklistItem(
    Graph::Node* node,
    const WrapperDescriptor& wrapper_descriptor) const {
  return {node, wrapper_descriptor.traceable,
          wrapper_descriptor.trace_wrappers_callback};
}

void V8EmbedderGraphBuilder::VisitTransitiveClosure() {
  // Depth-first search.
  while (!worklist_.empty()) {
    auto item = worklist_.back();
    worklist_.pop_back();
    ParentScope parent(this, item.node);
    item.trace_wrappers_callback(this, item.traceable);
  }
}

}  // namespace blink
