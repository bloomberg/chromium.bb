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

#ifndef NodeRenderingContext_h
#define NodeRenderingContext_h

#include "core/dom/NodeRenderingTraversal.h"

#include "wtf/RefPtr.h"

namespace WebCore {

class ContainerNode;
class Node;
class RenderNamedFlowThread;
class RenderObject;
class RenderStyle;

class NodeRenderingContext {
public:
    explicit NodeRenderingContext(Node* node, RenderStyle* style = 0)
        : m_node(node)
        , m_renderingParent(0)
        , m_style(style)
        , m_parentFlowRenderer(0)
    {
        m_renderingParent = NodeRenderingTraversal::parent(node, &m_parentDetails);
    }

    void createRendererForTextIfNeeded();
    void createRendererForElementIfNeeded();

    Node* node() const { return m_node; }
    RenderObject* parentRenderer() const;
    RenderObject* nextRenderer() const;
    RenderObject* previousRenderer() const;

    const RenderStyle* style() const { return m_style.get(); }

private:
    bool shouldCreateRenderer() const;
    void moveToFlowThreadIfNeeded();
    bool elementInsideRegionNeedsRenderer();

    Node* m_node;
    ContainerNode* m_renderingParent;
    NodeRenderingTraversal::ParentDetails m_parentDetails;
    RefPtr<RenderStyle> m_style;
    RenderNamedFlowThread* m_parentFlowRenderer;
};

} // namespace WebCore

#endif
