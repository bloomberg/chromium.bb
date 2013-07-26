/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#ifndef ElementResolveContext_h
#define ElementResolveContext_h

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/rendering/style/RenderStyleConstants.h"

namespace WebCore {

class ContainerNode;
class Element;
class RenderStyle;

// ElementResolveContext is immutable and serves as an input to the style resolve process.
class ElementResolveContext {
public:
    ElementResolveContext()
        : m_element(0)
        , m_parentNode(0)
        , m_rootElementStyle(0)
        , m_elementLinkState(NotInsideLink)
        , m_distributedToInsertionPoint(false)
        , m_resetStyleInheritance(false)
    {
    }

    explicit ElementResolveContext(Element*);

    Document* document() const { return m_element->document(); }
    bool isDocumentElement() const { return m_element && m_element == m_element->document()->documentElement(); }

    Element* element() const { return m_element; }
    const ContainerNode* parentNode() const { return m_parentNode; }
    RenderStyle* rootElementStyle() const { return m_rootElementStyle; }
    EInsideLink elementLinkState() const { return m_elementLinkState; }
    bool distributedToInsertionPoint() const { return m_distributedToInsertionPoint; }
    bool resetStyleInheritance() const { return m_resetStyleInheritance; }

private:
    Element* m_element;
    ContainerNode* m_parentNode;
    RenderStyle* m_rootElementStyle;
    EInsideLink m_elementLinkState;
    bool m_distributedToInsertionPoint;
    bool m_resetStyleInheritance;
};

} // namespace WebCore

#endif // ElementResolveContext_h
