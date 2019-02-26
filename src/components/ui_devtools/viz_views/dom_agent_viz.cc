// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/viz_views/dom_agent_viz.h"

#include "base/stl_util.h"
#include "components/ui_devtools/root_element.h"
#include "components/ui_devtools/ui_element.h"
#include "components/ui_devtools/viz_views/frame_sink_element.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"

namespace ui_devtools {

// Updating logic for FrameSinks:
//
// 1. Creating. We register a FrameSinkId, create a CompositorFrameSink and if a
// CompositorFrameSink is not a root we register hierarchy from the parent of
// this FrameSink to this CompositorFrameSink. When we register a FrameSinkId,
// we check if its corresponding element is already in the tree. If not, we
// attach it to the RootElement which serves as the root of the
// CompositorFrameSink tree. In this state the CompositorFrameSink is considered
// unembedded and it is a sibling of RootCompositorFrameSinks. If it is present
// in a tree we just change the properties (|has_created_frame_sink_|).
// These events don't know anything about the hierarchy
// so we don't change it. When we get OnRegisteredHierarchy from parent to child
// the corresponding elements must already be present in a tree. The usual state
// is: child is attached to RootElement and now we will detach it from the
// RootElement and attach to the real parent. During handling this event we
// actually delete the subtree of RootElement rooted from child and create a new
// subtree of parent. This potentially could be more efficient, but if we just
// switch necessary pointers we must send a notification to the frontend so it
// can update the UI. Unfortunately, this action involves deleting a node from
// the backend list of UI Elements (even when it is still alive) and trying to
// delete it once again (for instance, when we close a corresponding tab) causes
// crash.
//
// 2. Deleting. We unregister hierarchy, destroy a CompositorFrameSink and
// invalidate a FrameSinkId. When we invalidate an FrameSinkId or destroy a
// FrameSink we check if it's the last action that has to happen with the
// corresponding element. For example, if the element has
// |has_created_frame_sink_| = true and we get a |OnDestroyedFrameSink| event we
// just set |has_created_frame_sink_| = false, but don't remove it from a tree,
// because its FrameSinkId is still registered, so it's not completely dead. But
// when after that we get |OnInvalidatedFrameSinkId| we can remove the
// node from the tree. When we get OnUnregisteredHierarchy we assume the nodes
// are still present in a tree, so we do the same work as we did in registering
// case. Only here we move a subtree of parent rooted from child to the
// RootElement. Obviously, now the child will be in detached state.

using namespace ui_devtools::protocol;

DOMAgentViz::DOMAgentViz(viz::FrameSinkManagerImpl* frame_sink_manager)
    : frame_sink_manager_(frame_sink_manager) {}

DOMAgentViz::~DOMAgentViz() {
  Clear();
}

void DOMAgentViz::OnRegisteredFrameSinkId(
    const viz::FrameSinkId& frame_sink_id) {
  if (!base::ContainsKey(frame_sink_elements_, frame_sink_id)) {
    // If a FrameSink was just registered we don't know anything about
    // hierarchy. So we should attach it to the RootElement.
    element_root()->AddChild(
        new FrameSinkElement(frame_sink_id, frame_sink_manager_, this,
                             element_root(), /*is_root=*/false,
                             /*has_created_frame_sink=*/false));
  }
}

void DOMAgentViz::OnInvalidatedFrameSinkId(
    const viz::FrameSinkId& frame_sink_id) {
  auto it = frame_sink_elements_.find(frame_sink_id);
  DCHECK(it != frame_sink_elements_.end());

  // A FrameSinkElement with |frame_sink_id| can only be invalidated after
  // being destroyed.
  DCHECK(!it->second->has_created_frame_sink());
  DestroyElementAndRemoveSubtree(it->second);
}

void DOMAgentViz::OnCreatedCompositorFrameSink(
    const viz::FrameSinkId& frame_sink_id,
    bool is_root) {
  auto it = frame_sink_elements_.find(frame_sink_id);
  DCHECK(it != frame_sink_elements_.end());
  // The corresponding element is already present in a tree, so we
  // should update its |has_created_frame_sink_| and |is_root_| properties.
  it->second->SetHasCreatedFrameSink(true);
  it->second->SetRoot(is_root);
}

void DOMAgentViz::OnDestroyedCompositorFrameSink(
    const viz::FrameSinkId& frame_sink_id) {
  auto it = frame_sink_elements_.find(frame_sink_id);
  DCHECK(it != frame_sink_elements_.end());

  // Set FrameSinkElement to not connected to mark it as destroyed.
  it->second->SetHasCreatedFrameSink(false);
}

void DOMAgentViz::OnRegisteredFrameSinkHierarchy(
    const viz::FrameSinkId& parent_frame_sink_id,
    const viz::FrameSinkId& child_frame_sink_id) {
  // At this point these elements must be present in a tree.
  // We should detach a child from its current parent and attach to the new
  // parent.
  auto it_parent = frame_sink_elements_.find(parent_frame_sink_id);
  auto it_child = frame_sink_elements_.find(child_frame_sink_id);
  DCHECK(it_parent != frame_sink_elements_.end());
  DCHECK(it_child != frame_sink_elements_.end());

  FrameSinkElement* child = it_child->second;
  FrameSinkElement* new_parent = it_parent->second;

  // TODO: Add support for |child| to have multiple parents.
  Reparent(new_parent, child);
}

void DOMAgentViz::OnUnregisteredFrameSinkHierarchy(
    const viz::FrameSinkId& parent_frame_sink_id,
    const viz::FrameSinkId& child_frame_sink_id) {
  // At this point these elements must be present in a tree.
  // We should detach a child from its current parent and attach to the
  // RootElement since it wasn't destroyed yet.
  auto it_child = frame_sink_elements_.find(child_frame_sink_id);
  DCHECK(it_child != frame_sink_elements_.end());

  FrameSinkElement* child = it_child->second;

  // TODO: Add support for |child| to have multiple parents: only adds |child|
  // to RootElement if all parents of |child| are unregistered.
  Reparent(element_root(), child);
}

std::unique_ptr<DOM::Node> DOMAgentViz::BuildTreeForFrameSink(
    FrameSinkElement* frame_sink_element,
    const viz::FrameSinkId& frame_sink_id) {
  frame_sink_elements_.emplace(frame_sink_id, frame_sink_element);
  std::unique_ptr<Array<DOM::Node>> children = Array<DOM::Node>::create();

  // Once the FrameSinkElement is created it calls this function to build its
  // subtree. So we iterate through |frame_sink_element|'s children and
  // recursively build the subtree for them.
  for (auto& child : frame_sink_manager_->GetChildrenByParent(frame_sink_id)) {
    bool has_created_frame_sink =
        !!frame_sink_manager_->GetFrameSinkForId(child);

    FrameSinkElement* f_s_element = new FrameSinkElement(
        child, frame_sink_manager_, this, frame_sink_element,
        /*is_root=*/false, has_created_frame_sink);

    children->addItem(BuildTreeForFrameSink(f_s_element, child));
    frame_sink_element->AddChild(f_s_element);
  }
  std::unique_ptr<DOM::Node> node =
      BuildNode("FrameSink", frame_sink_element->GetAttributes(),
                std::move(children), frame_sink_element->node_id());
  return node;
}

protocol::Response DOMAgentViz::enable() {
  frame_sink_manager_->AddObserver(this);
  return protocol::Response::OK();
}

protocol::Response DOMAgentViz::disable() {
  frame_sink_manager_->RemoveObserver(this);
  Clear();
  element_root()->ClearChildren();
  return DOMAgent::disable();
}

std::vector<UIElement*> DOMAgentViz::CreateChildrenForRoot() {
  std::vector<UIElement*> children;

  // Find all elements that are not part of any hierarchy. This will be
  // FrameSinks that are either root, or detached.
  std::vector<viz::FrameSinkId> registered_frame_sinks =
      frame_sink_manager_->GetRegisteredFrameSinkIds();
  base::flat_set<viz::FrameSinkId> detached_frame_sinks(registered_frame_sinks);

  for (auto& frame_sink_id : registered_frame_sinks) {
    for (auto& child_id :
         frame_sink_manager_->GetChildrenByParent(frame_sink_id)) {
      detached_frame_sinks.erase(child_id);
    }
  }

  // Add created RootFrameSinks and detached FrameSinks.
  for (auto& frame_sink_id : detached_frame_sinks) {
    const viz::CompositorFrameSinkSupport* support =
        frame_sink_manager_->GetFrameSinkForId(frame_sink_id);
    bool is_root = support && support->is_root();
    bool has_created_frame_sink = !!support;
    // TODO(sgilhuly): Use unique_ptr instead of new for the FrameSinkElements.
    UIElement* frame_sink_element =
        new FrameSinkElement(frame_sink_id, frame_sink_manager_, this,
                             element_root(), is_root, has_created_frame_sink);
    children.push_back(frame_sink_element);
  }

  return children;
}

std::unique_ptr<DOM::Node> DOMAgentViz::BuildTreeForUIElement(
    UIElement* ui_element) {
  if (ui_element->type() == UIElementType::FRAMESINK) {
    FrameSinkElement* frame_sink_element =
        static_cast<FrameSinkElement*>(ui_element);
    return BuildTreeForFrameSink(frame_sink_element,
                                 frame_sink_element->frame_sink_id());
  }
  return nullptr;
}

void DOMAgentViz::Clear() {
  for (auto& entry : frame_sink_elements_) {
    entry.second->ClearChildren();
    delete entry.second;
  }
  frame_sink_elements_.clear();
}

void DOMAgentViz::DestroyElementAndRemoveSubtree(UIElement* element) {
  // We may come across the case where we've got the event to delete the
  // FrameSink, but we haven't got events to delete its children. We should
  // detach all its children and attach them to RootElement and then delete the
  // node we were asked for.
  for (auto* child : element->children())
    Reparent(element_root(), child);

  element->parent()->RemoveChild(element);
  DestroyElement(element);
}

void DOMAgentViz::Reparent(UIElement* new_parent, UIElement* child) {
  DestroySubtree(child);
  child->ClearChildren();

  // This removes the child element from the Node map. It has to be added with
  // null parent to recreate the entry.
  child->parent()->RemoveChild(child);
  OnUIElementAdded(nullptr, child);
  new_parent->AddChild(child);
  child->set_parent(new_parent);
}

void DOMAgentViz::DestroyElement(UIElement* element) {
  if (element->type() == UIElementType::FRAMESINK) {
    // TODO(sgilhuly): Use unique_ptr, so that the element doesn't need to be
    // deleted manually.
    frame_sink_elements_.erase(FrameSinkElement::From(element));
    delete element;
  } else {
    NOTREACHED();
  }
}

void DOMAgentViz::DestroySubtree(UIElement* element) {
  for (auto* child : element->children()) {
    DestroySubtree(child);
    DestroyElement(child);
  }
  element->ClearChildren();
}

}  // namespace ui_devtools
