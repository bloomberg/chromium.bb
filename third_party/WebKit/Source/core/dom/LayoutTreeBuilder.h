/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef LayoutTreeBuilder_h
#define LayoutTreeBuilder_h

#include "core/dom/Document.h"
#include "core/dom/FirstLetterPseudoElement.h"
#include "core/dom/Node.h"
#include "core/dom/NodeRenderingTraversal.h"
#include "core/dom/Text.h"
#include "core/layout/LayoutObject.h"
#include "wtf/RefPtr.h"

namespace blink {

class LayoutObject;
class LayoutStyle;

template <typename NodeType>
class LayoutTreeBuilder {
    STACK_ALLOCATED();
protected:
    LayoutTreeBuilder(NodeType& node, LayoutObject* renderingParent)
        : m_node(node)
        , m_renderingParent(renderingParent)
    {
        ASSERT(!node.renderer());
        ASSERT(node.needsAttach());
        ASSERT(node.document().inStyleRecalc());

        // FIXME: We should be able to ASSERT(node->inActiveDocument()) but childrenChanged is called
        // before ChildNodeInsertionNotifier in ContainerNode's methods and some implementations
        // will trigger a layout inside childrenChanged.
        // Mainly HTMLTextAreaElement::childrenChanged calls HTMLTextFormControlElement::setSelectionRange
        // which does an updateLayoutIgnorePendingStylesheets.
    }

    LayoutObject* parentRenderer() const { return m_renderingParent; }

    LayoutObject* nextRenderer() const
    {
        ASSERT(m_renderingParent);

        // Avoid an O(N^2) walk over the children when reattaching all children of a node.
        if (m_renderingParent->node() && m_renderingParent->node()->needsAttach())
            return 0;

        return NodeRenderingTraversal::nextSiblingRenderer(*m_node);
    }

    RawPtrWillBeMember<NodeType> m_node;
    RawPtrWillBeMember<LayoutObject> m_renderingParent;
};

class LayoutTreeBuilderForElement : public LayoutTreeBuilder<Element> {
public:
    LayoutTreeBuilderForElement(Element&, LayoutStyle*);

    void createRendererIfNeeded()
    {
        if (shouldCreateRenderer())
            createRenderer();
    }

private:
    LayoutObject* parentRenderer() const;
    LayoutObject* nextRenderer() const;
    bool shouldCreateRenderer() const;
    LayoutStyle& style() const;
    void createRenderer();

    mutable RefPtr<LayoutStyle> m_style;
};

class LayoutTreeBuilderForText : public LayoutTreeBuilder<Text> {
public:
    LayoutTreeBuilderForText(Text& text, LayoutObject* renderingParent)
        : LayoutTreeBuilder(text, renderingParent) { }

    void createRenderer();
};

} // namespace blink

#endif
