// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorAccessibilityAgent_h
#define InspectorAccessibilityAgent_h

#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/protocol/Accessibility.h"
#include "modules/ModulesExport.h"

namespace blink {

class AXObject;
class AXObjectCacheImpl;
class InspectorDOMAgent;
class Page;

using protocol::Accessibility::AXNode;
using protocol::Accessibility::AXNodeId;

class MODULES_EXPORT InspectorAccessibilityAgent
    : public InspectorBaseAgent<protocol::Accessibility::Metainfo> {
  WTF_MAKE_NONCOPYABLE(InspectorAccessibilityAgent);

 public:
  InspectorAccessibilityAgent(Page*, InspectorDOMAgent*);

  // Base agent methods.
  DECLARE_VIRTUAL_TRACE();

  // Protocol methods.
  protocol::Response getPartialAXTree(
      int dom_node_id,
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
};

}  // namespace blink

#endif  // InspectorAccessibilityAgent_h
