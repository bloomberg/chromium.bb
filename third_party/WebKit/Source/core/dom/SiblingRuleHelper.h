// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SiblingRuleHelper_h
#define SiblingRuleHelper_h

#include "core/dom/Node.h"

namespace WebCore {

class SiblingRuleHelper {
public:
    SiblingRuleHelper(Node* node) : m_node(node)
    {
        ASSERT(node);
        ASSERT(node->isElementNode() || node->isShadowRoot());
    }

    void checkForChildrenAdjacentRuleChanges();

    void setChildrenAffectedByDirectAdjacentRules();
    void setChildrenAffectedByForwardPositionalRules();
    void setChildrenAffectedByBackwardPositionalRules();
    void setChildrenAffectedByFirstChildRules();
    void setChildrenAffectedByLastChildRules();

    bool isFinishedParsingChildren();

    bool childrenSupportStyleSharing();

private:
    bool childrenAffectedByPositionalRules() const;
    bool childrenAffectedByFirstChildRules() const;
    bool childrenAffectedByLastChildRules() const;
    bool childrenAffectedByDirectAdjacentRules() const;
    bool childrenAffectedByForwardPositionalRules() const;
    bool childrenAffectedByBackwardPositionalRules() const;

    Node* m_node;
};

} // namespace

#endif
