// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_INSPECTORACCESSIBILITYAGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_INSPECTORACCESSIBILITYAGENT_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/inspector/InspectorBaseAgent.h"
#include "third_party/blink/renderer/core/inspector/protocol/Accessibility.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class AXObject;
class AXObjectCacheImpl;
class InspectorDOMAgent;
class Page;

using protocol::Accessibility::AXNode;
using protocol::Accessibility::AXNodeId;

class MODULES_EXPORT InspectorAccessibilityAgent
    : public InspectorBaseAgent<protocol::Accessibility::Metainfo> {
 public:
  InspectorAccessibilityAgent(Page*, InspectorDOMAgent*);

  // Base agent methods.
  virtual void Trace(blink::Visitor*);

  // Protocol methods.
  protocol::Response getPartialAXTree(
      protocol::Maybe<int> dom_node_id,
      protocol::Maybe<int> backend_node_id,
      protocol::Maybe<String> object_id,
      protocol::Maybe<bool> fetch_relatives,
      std::unique_ptr<protocol::Array<protocol::Accessibility::AXNode>>*)
      override;

 private:
  std::unique_ptr<AXNode> BuildObjectForIgnoredNode(
      Node* dom_node,
      AXObject*,
      bool fetch_relatives,
      std::unique_ptr<protocol::Array<AXNode>>& nodes,
      AXObjectCacheImpl&) const;
  void PopulateDOMNodeAncestors(Node& inspected_dom_node,
                                AXNode&,
                                std::unique_ptr<protocol::Array<AXNode>>& nodes,
                                AXObjectCacheImpl&) const;
  std::unique_ptr<AXNode> BuildProtocolAXObject(
      AXObject&,
      AXObject* inspected_ax_object,
      bool fetch_relatives,
      std::unique_ptr<protocol::Array<AXNode>>& nodes,
      AXObjectCacheImpl&) const;
  void FillCoreProperties(AXObject&,
                          AXObject* inspected_ax_object,
                          bool fetch_relatives,
                          AXNode&,
                          std::unique_ptr<protocol::Array<AXNode>>& nodes,
                          AXObjectCacheImpl&) const;
  void AddAncestors(AXObject& first_ancestor,
                    AXObject* inspected_ax_object,
                    std::unique_ptr<protocol::Array<AXNode>>& nodes,
                    AXObjectCacheImpl&) const;
  void PopulateRelatives(AXObject&,
                         AXObject* inspected_ax_object,
                         AXNode&,
                         std::unique_ptr<protocol::Array<AXNode>>& nodes,
                         AXObjectCacheImpl&) const;
  void AddSiblingsOfIgnored(
      std::unique_ptr<protocol::Array<AXNodeId>>& child_ids,
      AXObject& parent_ax_object,
      AXObject* inspected_ax_object,
      std::unique_ptr<protocol::Array<AXNode>>& nodes,
      AXObjectCacheImpl&) const;
  void addChild(std::unique_ptr<protocol::Array<AXNodeId>>& child_ids,
                AXObject& child_ax_object,
                AXObject* inspected_ax_object,
                std::unique_ptr<protocol::Array<AXNode>>& nodes,
                AXObjectCacheImpl&) const;
  void AddChildren(AXObject&,
                   AXObject* inspected_ax_object,
                   std::unique_ptr<protocol::Array<AXNodeId>>& child_ids,
                   std::unique_ptr<protocol::Array<AXNode>>& nodes,
                   AXObjectCacheImpl&) const;

  Member<Page> page_;
  Member<InspectorDOMAgent> dom_agent_;

  DISALLOW_COPY_AND_ASSIGN(InspectorAccessibilityAgent);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_INSPECTORACCESSIBILITYAGENT_H_
