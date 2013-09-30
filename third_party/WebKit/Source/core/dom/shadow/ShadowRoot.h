/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ShadowRoot_h
#define ShadowRoot_h

#include "core/dom/ContainerNode.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/Element.h"
#include "core/dom/TreeScope.h"
#include "wtf/DoublyLinkedList.h"

namespace WebCore {

class ElementShadow;
class ExceptionState;
class InsertionPoint;
class ShadowRootRareData;

class ShadowRoot FINAL : public DocumentFragment, public TreeScope, public DoublyLinkedListNode<ShadowRoot> {
    friend class WTF::DoublyLinkedListNode<ShadowRoot>;
public:
    // FIXME: We will support multiple shadow subtrees, however current implementation does not work well
    // if a shadow root is dynamically created. So we prohibit multiple shadow subtrees
    // in several elements for a while.
    // See https://bugs.webkit.org/show_bug.cgi?id=77503 and related bugs.
    enum ShadowRootType {
        UserAgentShadowRoot = 0,
        AuthorShadowRoot
    };

    static PassRefPtr<ShadowRoot> create(Document* document, ShadowRootType type)
    {
        return adoptRef(new ShadowRoot(document, type));
    }

    void recalcStyle(StyleRecalcChange);

    Element* host() const { return toElement(parentOrShadowHostNode()); }
    ElementShadow* owner() const { return host() ? host()->shadow() : 0; }

    ShadowRoot* youngerShadowRoot() const { return prev(); }

    ShadowRoot* bindingsOlderShadowRoot() const;
    bool shouldExposeToBindings() const { return type() == AuthorShadowRoot; }

    bool isYoungest() const { return !youngerShadowRoot(); }
    bool isOldest() const { return !olderShadowRoot(); }
    bool isOldestAuthorShadowRoot() const;

    virtual void attach(const AttachContext& = AttachContext()) OVERRIDE;

    virtual InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE;
    virtual void removedFrom(ContainerNode*) OVERRIDE;

    virtual void registerScopedHTMLStyleChild() OVERRIDE;
    virtual void unregisterScopedHTMLStyleChild() OVERRIDE;

    bool containsShadowElements() const;
    bool containsContentElements() const;
    bool containsInsertionPoints() const { return containsShadowElements() || containsContentElements(); }
    bool containsShadowRoots() const;

    // For Internals, don't use this.
    unsigned childShadowRootCount() const;

    InsertionPoint* insertionPoint() const;
    void setInsertionPoint(PassRefPtr<InsertionPoint>);

    void addInsertionPoint(InsertionPoint*);
    void removeInsertionPoint(InsertionPoint*);
    const Vector<RefPtr<InsertionPoint> >& childInsertionPoints();

    ShadowRootType type() const { return static_cast<ShadowRootType>(m_type); }

public:
    Element* activeElement() const;

    bool applyAuthorStyles() const { return m_applyAuthorStyles; }
    void setApplyAuthorStyles(bool);

    bool resetStyleInheritance() const { return m_resetStyleInheritance; }
    void setResetStyleInheritance(bool);

    ShadowRoot* olderShadowRoot() const { return next(); }

    String innerHTML() const;
    void setInnerHTML(const String&, ExceptionState&);

    PassRefPtr<Node> cloneNode(bool, ExceptionState&);
    PassRefPtr<Node> cloneNode(ExceptionState& es) { return cloneNode(true, es); }

    StyleSheetList* styleSheets();

private:
    ShadowRoot(Document*, ShadowRootType);
    virtual ~ShadowRoot();

    virtual void dispose() OVERRIDE;
    virtual bool childTypeAllowed(NodeType) const OVERRIDE;
    virtual void childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta) OVERRIDE;

    ShadowRootRareData* ensureShadowRootRareData();

    void addChildShadowRoot();
    void removeChildShadowRoot();
    void invalidateChildInsertionPoints();

    // ShadowRoots should never be cloned.
    virtual PassRefPtr<Node> cloneNode(bool) OVERRIDE { return 0; }

    // FIXME: This shouldn't happen. https://bugs.webkit.org/show_bug.cgi?id=88834
    bool isOrphan() const { return !host(); }
    bool isActive() const;

    ShadowRoot* m_prev;
    ShadowRoot* m_next;
    OwnPtr<ShadowRootRareData> m_shadowRootRareData;
    unsigned m_numberOfStyles : 27;
    unsigned m_applyAuthorStyles : 1;
    unsigned m_resetStyleInheritance : 1;
    unsigned m_type : 1;
    unsigned m_registeredWithParentShadowRoot : 1;
    unsigned m_childInsertionPointsIsValid : 1;
};

inline Element* ShadowRoot::activeElement() const
{
    if (Element* element = treeScope().adjustedFocusedElement())
        return element;
    return 0;
}

inline const ShadowRoot* toShadowRoot(const Node* node)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!node || node->isShadowRoot());
    return static_cast<const ShadowRoot*>(node);
}

inline ShadowRoot* toShadowRoot(Node* node)
{
    return const_cast<ShadowRoot*>(toShadowRoot(static_cast<const Node*>(node)));
}

inline const ShadowRoot& toShadowRoot(const Node& node)
{
    ASSERT_WITH_SECURITY_IMPLICATION(node.isShadowRoot());
    return static_cast<const ShadowRoot&>(node);
}

inline const ShadowRoot* toShadowRoot(const TreeScope* treeScope)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!treeScope || (treeScope->rootNode() && treeScope->rootNode()->isShadowRoot()));
    return static_cast<const ShadowRoot*>(treeScope);
}

inline ShadowRoot* toShadowRoot(TreeScope* treeScope)
{
    return const_cast<ShadowRoot*>(toShadowRoot(static_cast<const TreeScope*>(treeScope)));
}

inline ShadowRoot& toShadowRoot(TreeScope& treeScope)
{
    ASSERT_WITH_SECURITY_IMPLICATION(treeScope.rootNode() && treeScope.rootNode()->isShadowRoot());
    return static_cast<ShadowRoot&>(treeScope);
}

} // namespace

#endif
