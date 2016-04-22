// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorAccessibilityAgent_h
#define InspectorAccessibilityAgent_h

#include "core/inspector/InspectorBaseAgent.h"
#include "modules/ModulesExport.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class InspectorDOMAgent;
class Page;

class MODULES_EXPORT InspectorAccessibilityAgent : public InspectorBaseAgent<InspectorAccessibilityAgent, protocol::Frontend::Accessibility>, public protocol::Backend::Accessibility {
    WTF_MAKE_NONCOPYABLE(InspectorAccessibilityAgent);
public:
    InspectorAccessibilityAgent(Page*, InspectorDOMAgent*);

    // Base agent methods.
    DECLARE_VIRTUAL_TRACE();

    // Protocol methods.
    void getAXNode(ErrorString*, int nodeId, Maybe<protocol::Accessibility::AXNode>* accessibilityNode) override;

private:
    Member<Page> m_page;
    Member<InspectorDOMAgent> m_domAgent;
};

} // namespace blink

#endif // InspectorAccessibilityAgent_h
