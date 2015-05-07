/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012, Google, Inc. All rights reserved.
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
 */

#ifndef SiblingTraversalStrategies_h
#define SiblingTraversalStrategies_h

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/NthIndexCache.h"

namespace blink {

// TODO(esprehn): Merge this into ElementTraversal.
class DOMSiblingTraversalStrategy {
public:
    static bool isFirstChild(Element&);
    static bool isLastChild(Element&);
    static bool isFirstOfType(Element&, const QualifiedName&);
    static bool isLastOfType(Element&, const QualifiedName&);

    static int countElementsBefore(Element&);
    static int countElementsAfter(Element&);
    static int countElementsOfTypeBefore(Element&, const QualifiedName&);
    static int countElementsOfTypeAfter(Element&, const QualifiedName&);

private:
    class HasTagName {
    public:
        explicit HasTagName(const QualifiedName& tagName) : m_tagName(tagName) { }
        bool operator() (const Element& element) const { return element.hasTagName(m_tagName); }
    private:
        const QualifiedName& m_tagName;
    };
};

inline bool DOMSiblingTraversalStrategy::isFirstChild(Element& element)
{
    return !ElementTraversal::previousSibling(element);
}

inline bool DOMSiblingTraversalStrategy::isLastChild(Element& element)
{
    return !ElementTraversal::nextSibling(element);
}

inline bool DOMSiblingTraversalStrategy::isFirstOfType(Element& element, const QualifiedName& type)
{
    return !ElementTraversal::previousSibling(element, HasTagName(type));
}

inline bool DOMSiblingTraversalStrategy::isLastOfType(Element& element, const QualifiedName& type)
{
    return !ElementTraversal::nextSibling(element, HasTagName(type));
}

inline int DOMSiblingTraversalStrategy::countElementsBefore(Element& element)
{
    if (NthIndexCache* nthIndexCache = element.document().nthIndexCache())
        return nthIndexCache->nthChildIndex(element) - 1;

    int count = 0;
    for (const Element* sibling = ElementTraversal::previousSibling(element); sibling; sibling = ElementTraversal::previousSibling(*sibling))
        count++;

    return count;
}

inline int DOMSiblingTraversalStrategy::countElementsOfTypeBefore(Element& element, const QualifiedName& type)
{
    int count = 0;
    for (const Element* sibling = ElementTraversal::previousSibling(element, HasTagName(type)); sibling; sibling = ElementTraversal::previousSibling(*sibling, HasTagName(type)))
        ++count;
    return count;
}

inline int DOMSiblingTraversalStrategy::countElementsAfter(Element& element)
{
    if (NthIndexCache* nthIndexCache = element.document().nthIndexCache())
        return nthIndexCache->nthLastChildIndex(element) - 1;

    int count = 0;
    for (const Element* sibling = ElementTraversal::nextSibling(element); sibling; sibling = ElementTraversal::nextSibling(*sibling))
        ++count;
    return count;
}

inline int DOMSiblingTraversalStrategy::countElementsOfTypeAfter(Element& element, const QualifiedName& type)
{
    int count = 0;
    for (const Element* sibling = ElementTraversal::nextSibling(element, HasTagName(type)); sibling; sibling = ElementTraversal::nextSibling(*sibling, HasTagName(type)))
        ++count;
    return count;
}

}

#endif
