// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/dom/SiblingRuleHelper.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/shadow/ShadowRoot.h"

namespace WebCore {

bool SiblingRuleHelper::isFinishedParsingChildren()
{
    if (m_node->isElementNode())
        return toElement(m_node)->isFinishedParsingChildren();

    return toShadowRoot(m_node)->isFinishedParsingChildren();
}

void SiblingRuleHelper::setChildrenAffectedByDirectAdjacentRules()
{
    if (m_node->isElementNode())
        toElement(m_node)->setChildrenAffectedByDirectAdjacentRules();
    else
        toShadowRoot(m_node)->setChildrenAffectedByDirectAdjacentRules();
}

void SiblingRuleHelper::setChildrenAffectedByForwardPositionalRules()
{
    if (m_node->isElementNode())
        toElement(m_node)->setChildrenAffectedByForwardPositionalRules();
    else
        toShadowRoot(m_node)->setChildrenAffectedByForwardPositionalRules();
}

void SiblingRuleHelper::setChildrenAffectedByBackwardPositionalRules()
{
    if (m_node->isElementNode())
        toElement(m_node)->setChildrenAffectedByBackwardPositionalRules();
    else
        toShadowRoot(m_node)->setChildrenAffectedByBackwardPositionalRules();
}

void SiblingRuleHelper::setChildrenAffectedByFirstChildRules()
{
    if (m_node->isElementNode())
        toElement(m_node)->setChildrenAffectedByFirstChildRules();
    else
        toShadowRoot(m_node)->setChildrenAffectedByFirstChildRules();
}

void SiblingRuleHelper::setChildrenAffectedByLastChildRules()
{
    if (m_node->isElementNode())
        toElement(m_node)->setChildrenAffectedByLastChildRules();
    else
        toShadowRoot(m_node)->setChildrenAffectedByLastChildRules();
}

bool SiblingRuleHelper::childrenAffectedByPositionalRules() const
{
    return m_node->isElementNode() ? toElement(m_node)->childrenAffectedByPositionalRules() : toShadowRoot(m_node)->childrenAffectedByPositionalRules();
}

bool SiblingRuleHelper::childrenAffectedByFirstChildRules() const
{
    return m_node->isElementNode() ? toElement(m_node)->childrenAffectedByFirstChildRules() : toShadowRoot(m_node)->childrenAffectedByFirstChildRules();
}

bool SiblingRuleHelper::childrenAffectedByLastChildRules() const
{
    return m_node->isElementNode() ? toElement(m_node)->childrenAffectedByLastChildRules() : toShadowRoot(m_node)->childrenAffectedByLastChildRules();
}

bool SiblingRuleHelper::childrenAffectedByDirectAdjacentRules() const
{
    return m_node->isElementNode() ? toElement(m_node)->childrenAffectedByDirectAdjacentRules() : toShadowRoot(m_node)->childrenAffectedByDirectAdjacentRules();
}

bool SiblingRuleHelper::childrenAffectedByForwardPositionalRules() const
{
    return m_node->isElementNode() ? toElement(m_node)->childrenAffectedByForwardPositionalRules() : toShadowRoot(m_node)->childrenAffectedByForwardPositionalRules();
}

bool SiblingRuleHelper::childrenAffectedByBackwardPositionalRules() const
{
    return m_node->isElementNode() ? toElement(m_node)->childrenAffectedByBackwardPositionalRules() : toShadowRoot(m_node)->childrenAffectedByBackwardPositionalRules();
}

void SiblingRuleHelper::checkForChildrenAdjacentRuleChanges()
{
    bool hasDirectAdjacentRules = childrenAffectedByDirectAdjacentRules();
    bool hasIndirectAdjacentRules = childrenAffectedByForwardPositionalRules();

    if (!hasDirectAdjacentRules && !hasIndirectAdjacentRules)
        return;

    unsigned forceCheckOfNextElementCount = 0;
    bool forceCheckOfAnyElementSibling = false;
    Document& document = m_node->document();

    for (Node* child = m_node->firstChild(); child; child = child->nextSibling()) {
        if (!child->isElementNode())
            continue;
        Element* element = toElement(child);
        bool childRulesChanged = element->needsStyleRecalc() && element->styleChangeType() >= SubtreeStyleChange;

        if (forceCheckOfNextElementCount || forceCheckOfAnyElementSibling)
            element->setNeedsStyleRecalc(SubtreeStyleChange);

        if (forceCheckOfNextElementCount)
            forceCheckOfNextElementCount--;

        if (childRulesChanged && hasDirectAdjacentRules)
            forceCheckOfNextElementCount = document.styleEngine()->maxDirectAdjacentSelectors();

        forceCheckOfAnyElementSibling = forceCheckOfAnyElementSibling || (childRulesChanged && hasIndirectAdjacentRules);
    }
}

bool SiblingRuleHelper::childrenSupportStyleSharing()
{
    return m_node->isElementNode() ? toElement(m_node)->childrenSupportStyleSharing() : toShadowRoot(m_node)->childrenSupportStyleSharing();
}

} // namespace WebCore

