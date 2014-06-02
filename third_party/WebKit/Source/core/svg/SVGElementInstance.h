/*
 * Copyright (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2011 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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

#ifndef SVGElementInstance_h
#define SVGElementInstance_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/TreeShared.h"

namespace WebCore {

namespace Private {
template<class GenericNode, class GenericNodeContainer>
void addChildNodesToDeletionQueue(GenericNode*& head, GenericNode*& tail, GenericNodeContainer&);
};

class Document;
class SVGElement;
class SVGUseElement;

// SVGElementInstance mimics Node, but without providing all its functionality
class SVGElementInstance FINAL : public TreeSharedWillBeRefCountedGarbageCollected<SVGElementInstance>, public ScriptWrappable {
public:
    static PassRefPtrWillBeRawPtr<SVGElementInstance> create(SVGUseElement* correspondingUseElement, SVGUseElement* directUseElement, PassRefPtrWillBeRawPtr<SVGElement> originalElement);

    virtual ~SVGElementInstance();

    void setParentOrShadowHostNode(SVGElementInstance* instance) { m_parentInstance = instance; }

    SVGElement* correspondingElement() const { return m_element.get(); }
    SVGUseElement* correspondingUseElement() const { return m_correspondingUseElement; }
    SVGUseElement* directUseElement() const { return m_directUseElement; }
    SVGElement* shadowTreeElement() const { return m_shadowTreeElement.get(); }

    void detach();

    SVGElementInstance* parentNode() const { return m_parentInstance; }

    SVGElementInstance* previousSibling() const { return m_previousSibling; }
    SVGElementInstance* nextSibling() const { return m_nextSibling; }

    SVGElementInstance* firstChild() const { return m_firstChild; }
    SVGElementInstance* lastChild() const { return m_lastChild; }

    inline Document* ownerDocument() const;

    virtual void trace(Visitor*);

private:
    friend class SVGUseElement;
    friend class TreeShared<SVGElementInstance>;

    SVGElementInstance(SVGUseElement*, SVGUseElement*, PassRefPtrWillBeRawPtr<SVGElement> originalElement);

#if !ENABLE(OILPAN)
    void removedLastRef();
#endif

    bool hasTreeSharedParent() const { return !!m_parentInstance; }

    void appendChild(PassRefPtrWillBeRawPtr<SVGElementInstance> child);
    void setShadowTreeElement(SVGElement*);

    template<class GenericNode, class GenericNodeContainer>
    friend void appendChildToContainer(GenericNode& child, GenericNodeContainer&);

#if !ENABLE(OILPAN)
    template<class GenericNode, class GenericNodeContainer>
    friend void removeDetachedChildrenInContainer(GenericNodeContainer&);

    template<class GenericNode, class GenericNodeContainer>
    friend void Private::addChildNodesToDeletionQueue(GenericNode*& head, GenericNode*& tail, GenericNodeContainer&);
#endif

    bool hasChildren() const { return m_firstChild; }

    void setFirstChild(SVGElementInstance* child) { m_firstChild = child; }
    void setLastChild(SVGElementInstance* child) { m_lastChild = child; }

    void setNextSibling(SVGElementInstance* sibling) { m_nextSibling = sibling; }
    void setPreviousSibling(SVGElementInstance* sibling) { m_previousSibling = sibling; }

    RawPtrWillBeMember<SVGElementInstance> m_parentInstance;

    RawPtrWillBeMember<SVGUseElement> m_correspondingUseElement;
    RawPtrWillBeMember<SVGUseElement> m_directUseElement;
    RefPtrWillBeMember<SVGElement> m_element;
    RefPtrWillBeMember<SVGElement> m_shadowTreeElement;

    RawPtrWillBeMember<SVGElementInstance> m_previousSibling;
    RawPtrWillBeMember<SVGElementInstance> m_nextSibling;

    RawPtrWillBeMember<SVGElementInstance> m_firstChild;
    RawPtrWillBeMember<SVGElementInstance> m_lastChild;
};

} // namespace WebCore

#endif
