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
class HTMLShadowElement;
class InsertionPoint;
class ShadowRootRareData;

class ShadowRoot FINAL : public DocumentFragment, public TreeScope, public DoublyLinkedListNode<ShadowRoot> {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(ShadowRoot);
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

    static PassRefPtr<ShadowRoot> create(Document& document, ShadowRootType type)
    {
        return adoptRef(new ShadowRoot(document, type));
    }

    void recalcStyle(StyleRecalcChange);

    // Disambiguate between Node and TreeScope hierarchies; TreeScope's implementation is simpler.
    using TreeScope::document;

    Element* host() const { return toElement(parentOrShadowHostNode()); }
    ElementShadow* owner() const { return host() ? host()->shadow() : 0; }

    ShadowRoot* youngerShadowRoot() const { return prev(); }

    ShadowRoot* olderShadowRootForBindings() const;
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

    unsigned descendantShadowElementCount() const;

    // For Internals, don't use this.
    unsigned childShadowRootCount() const;

    HTMLShadowElement* shadowInsertionPointOfYoungerShadowRoot() const;
    void setShadowInsertionPointOfYoungerShadowRoot(PassRefPtr<HTMLShadowElement>);

    void didAddInsertionPoint(InsertionPoint*);
    void didRemoveInsertionPoint(InsertionPoint*);
    const Vector<RefPtr<InsertionPoint> >& descendantInsertionPoints();

    ShadowRootType type() const { return static_cast<ShadowRootType>(m_type); }

    // Make protected methods from base class public here.
    using TreeScope::setDocument;
    using TreeScope::setParentTreeScope;

public:
    Element* activeElement() const;

    bool applyAuthorStyles() const { return m_applyAuthorStyles; }
    void setApplyAuthorStyles(bool);

    ShadowRoot* olderShadowRoot() const { return next(); }

    String innerHTML() const;
    void setInnerHTML(const String&, ExceptionState&);

    PassRefPtr<Node> cloneNode(bool, ExceptionState&);
    PassRefPtr<Node> cloneNode(ExceptionState& exceptionState) { return cloneNode(true, exceptionState); }

    StyleSheetList* styleSheets();

private:
    ShadowRoot(Document&, ShadowRootType);
    virtual ~ShadowRoot();

    virtual void dispose() OVERRIDE;
    virtual void childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta) OVERRIDE;

    ShadowRootRareData* ensureShadowRootRareData();

    void addChildShadowRoot();
    void removeChildShadowRoot();
    void invalidateDescendantInsertionPoints();

    // ShadowRoots should never be cloned.
    virtual PassRefPtr<Node> cloneNode(bool) OVERRIDE { return nullptr; }

    // FIXME: This shouldn't happen. https://bugs.webkit.org/show_bug.cgi?id=88834
    bool isOrphan() const { return !host(); }

    ShadowRoot* m_prev;
    ShadowRoot* m_next;
    OwnPtr<ShadowRootRareData> m_shadowRootRareData;
    unsigned m_numberOfStyles : 27;
    unsigned m_applyAuthorStyles : 1;
    unsigned m_type : 1;
    unsigned m_registeredWithParentShadowRoot : 1;
    unsigned m_descendantInsertionPointsIsValid : 1;
};

inline Element* ShadowRoot::activeElement() const
{
    return adjustedFocusedElement();
}

DEFINE_NODE_TYPE_CASTS(ShadowRoot, isShadowRoot());
DEFINE_TYPE_CASTS(ShadowRoot, TreeScope, treeScope, treeScope->rootNode().isShadowRoot(), treeScope.rootNode().isShadowRoot());

} // namespace

#endif
