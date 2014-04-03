/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
 * Copyright (C) 2012 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TreeScope_h
#define TreeScope_h

#include "core/dom/DocumentOrderedMap.h"
#include "heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/text/AtomicString.h"

namespace WebCore {

class ContainerNode;
class DOMSelection;
class Document;
class Element;
class HTMLLabelElement;
class HTMLMapElement;
class HitTestResult;
class LayoutPoint;
class IdTargetObserverRegistry;
class Node;
class RenderObject;

// A class which inherits both Node and TreeScope must call clearRareData() in its destructor
// so that the Node destructor no longer does problematic NodeList cache manipulation in
// the destructor.
class TreeScope : public WillBeGarbageCollectedMixin {
public:
    TreeScope* parentTreeScope() const { return m_parentTreeScope; }

    TreeScope* olderShadowRootOrParentTreeScope() const;
    bool isInclusiveOlderSiblingShadowRootOrAncestorTreeScopeOf(const TreeScope&) const;

    Element* adjustedFocusedElement() const;
    Element* getElementById(const AtomicString&) const;
    const Vector<Element*>& getAllElementsById(const AtomicString&) const;
    bool hasElementWithId(StringImpl* id) const;
    bool containsMultipleElementsWithId(const AtomicString& id) const;
    void addElementById(const AtomicString& elementId, Element*);
    void removeElementById(const AtomicString& elementId, Element*);

    Document& document() const
    {
        ASSERT(m_document);
        return *m_document;
    }

    Node* ancestorInThisScope(Node*) const;

    void addImageMap(HTMLMapElement*);
    void removeImageMap(HTMLMapElement*);
    HTMLMapElement* getImageMap(const String& url) const;

    Element* elementFromPoint(int x, int y) const;

    // For accessibility.
    bool shouldCacheLabelsByForAttribute() const { return m_labelsByForAttribute; }
    void addLabel(const AtomicString& forAttributeValue, HTMLLabelElement*);
    void removeLabel(const AtomicString& forAttributeValue, HTMLLabelElement*);
    HTMLLabelElement* labelElementForId(const AtomicString& forAttributeValue);

    DOMSelection* getSelection() const;

    // Find first anchor with the given name.
    // First searches for an element with the given ID, but if that fails, then looks
    // for an anchor with the given name. ID matching is always case sensitive, but
    // Anchor name matching is case sensitive in strict mode and not case sensitive in
    // quirks mode for historical compatibility reasons.
    Element* findAnchor(const String& name);

    bool applyAuthorStyles() const;

    // Used by the basic DOM mutation methods (e.g., appendChild()).
    void adoptIfNeeded(Node&);

    Node& rootNode() const { return m_rootNode; }

    IdTargetObserverRegistry& idTargetObserverRegistry() const { return *m_idTargetObserverRegistry.get(); }

    // Nodes belonging to this scope hold guard references -
    // these are enough to keep the scope from being destroyed, but
    // not enough to keep it from removing its children. This allows a
    // node that outlives its scope to still have a valid document
    // pointer without introducing reference cycles.
    void guardRef()
    {
#if ENABLE(OILPAN)
        if (!m_guardRefCount) {
            ASSERT(!m_keepAlive);
            m_keepAlive = adoptPtr(new Persistent<TreeScope>(this));
        }
#else
        ASSERT(!deletionHasBegun());
#endif
        ++m_guardRefCount;
    }

    void guardDeref()
    {
        ASSERT(m_guardRefCount > 0);
#if !ENABLE(OILPAN)
        ASSERT(!deletionHasBegun());
#endif
        --m_guardRefCount;
#if ENABLE(OILPAN)
        if (!m_guardRefCount)
            clearKeepAlive();
#else
        if (!m_guardRefCount && !refCount() && !rootNodeHasTreeSharedParent()) {
            beginDeletion();
            delete this;
        }
#endif
    }

    void removedLastRefToScope();

#if ENABLE(OILPAN)
    void clearKeepAlive()
    {
        ASSERT(m_keepAlive);
        m_keepAlive = nullptr;
    }
#endif

    bool isInclusiveAncestorOf(const TreeScope&) const;
    unsigned short comparePosition(const TreeScope&) const;

    Element* getElementByAccessKey(const String& key) const;

protected:
    TreeScope(ContainerNode&, Document&);
    TreeScope(Document&);
    virtual ~TreeScope();

    void destroyTreeScopeData();
    void setDocument(Document& document) { m_document = &document; }
    void setParentTreeScope(TreeScope&);

    bool hasGuardRefCount() const { return m_guardRefCount; }

    void setNeedsStyleRecalcForViewportUnits();

private:
    virtual void dispose() { }

    int refCount() const;
#if SECURITY_ASSERT_ENABLED
    bool deletionHasBegun();
    void beginDeletion();
#else
    bool deletionHasBegun() { return false; }
    void beginDeletion() { }
#endif

    bool rootNodeHasTreeSharedParent() const;

    Node& m_rootNode;
    Document* m_document;
    TreeScope* m_parentTreeScope;
    int m_guardRefCount;

    OwnPtr<DocumentOrderedMap> m_elementsById;
    OwnPtr<DocumentOrderedMap> m_imageMapsByName;
    OwnPtr<DocumentOrderedMap> m_labelsByForAttribute;

    OwnPtr<IdTargetObserverRegistry> m_idTargetObserverRegistry;

#if ENABLE(OILPAN)
    // With Oilpan, a non-zero reference count will keep the TreeScope alive
    // with a self-persistent handle. Whenever the ref count goes above zero
    // we register the TreeScope as a root for garbage collection by allocating a
    // persistent handle to the object itself. When the ref count goes to zero
    // we deallocate the persistent handle again so the object can die if there
    // are no other things keeping it alive.
    //
    // FIXME: Oilpan: Remove m_keepAlive and ref counting and use tracing instead.
    GC_PLUGIN_IGNORE("359444")
    OwnPtr<Persistent<TreeScope> > m_keepAlive;
#endif

    GC_PLUGIN_IGNORE("359444")
    mutable RefPtrWillBePersistent<DOMSelection> m_selection;
};

inline bool TreeScope::hasElementWithId(StringImpl* id) const
{
    ASSERT(id);
    return m_elementsById && m_elementsById->contains(id);
}

inline bool TreeScope::containsMultipleElementsWithId(const AtomicString& id) const
{
    return m_elementsById && m_elementsById->containsMultiple(id.impl());
}

inline bool operator==(const TreeScope& a, const TreeScope& b) { return &a == &b; }
inline bool operator==(const TreeScope& a, const TreeScope* b) { return &a == b; }
inline bool operator==(const TreeScope* a, const TreeScope& b) { return a == &b; }
inline bool operator!=(const TreeScope& a, const TreeScope& b) { return !(a == b); }
inline bool operator!=(const TreeScope& a, const TreeScope* b) { return !(a == b); }
inline bool operator!=(const TreeScope* a, const TreeScope& b) { return !(a == b); }

HitTestResult hitTestInDocument(const Document*, int x, int y);
TreeScope* commonTreeScope(Node*, Node*);

} // namespace WebCore

#endif // TreeScope_h
