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
class LocalFrame;
class Page;

class MODULES_EXPORT InspectorAccessibilityAgent
    : public InspectorBaseAgent<protocol::Accessibility::Metainfo> {
  WTF_MAKE_NONCOPYABLE(InspectorAccessibilityAgent);

 public:
  InspectorAccessibilityAgent(Page*, InspectorDOMAgent*);

  // Base agent methods.
  DECLARE_VIRTUAL_TRACE();

  // Protocol methods.
  void getAXNodeChain(
      ErrorString*,
      int domNodeId,
      bool fetchAncestors,
      std::unique_ptr<protocol::Array<protocol::Accessibility::AXNode>>*)
      override;

 private:
  Member<Page> m_page;
  Member<InspectorDOMAgent> m_domAgent;

  std::unique_ptr<protocol::Accessibility::AXNode> buildObjectForIgnoredNode(
      Node* domNode,
      AXObject*) const;
  std::unique_ptr<protocol::Accessibility::AXNode> buildProtocolAXObject(
      AXObject&) const;
  void fillCoreProperties(AXObject&, protocol::Accessibility::AXNode&) const;
};

}  // namespace blink

#endif  // InspectorAccessibilityAgent_h
