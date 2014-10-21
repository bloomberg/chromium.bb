/*
 * Copyright (C) 2014, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AXObjectCacheImpl_h
#define AXObjectCacheImpl_h

#include "core/accessibility/AXObject.h"
#include "core/accessibility/AXObjectCache.h"
#include "core/rendering/RenderText.h"
#include "platform/Timer.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/RefPtr.h"

namespace blink {

class AbstractInlineTextBox;
class HTMLAreaElement;
class FrameView;
class Widget;

struct TextMarkerData {
    AXID axID;
    Node* node;
    int offset;
    EAffinity affinity;
};

class AXComputedObjectAttributeCache {
public:
    static PassOwnPtr<AXComputedObjectAttributeCache> create() { return adoptPtr(new AXComputedObjectAttributeCache()); }

    AXObjectInclusion getIgnored(AXID) const;
    void setIgnored(AXID, AXObjectInclusion);

    void clear();

private:
    AXComputedObjectAttributeCache() { }

    struct CachedAXObjectAttributes {
        CachedAXObjectAttributes() : ignored(DefaultBehavior) { }

        AXObjectInclusion ignored;
    };

    HashMap<AXID, CachedAXObjectAttributes> m_idMapping;
};

// This class should only be used from inside the accessibility directory.
class AXObjectCacheImpl : public AXObjectCache {
    WTF_MAKE_NONCOPYABLE(AXObjectCacheImpl); WTF_MAKE_FAST_ALLOCATED;
public:
    explicit AXObjectCacheImpl(Document&);
    ~AXObjectCacheImpl();

    static AXObject* focusedUIElementForPage(const Page*);

    virtual AXObject* objectFromAXID(AXID id) const override { return m_objects.get(id); }

    virtual void selectionChanged(Node*) override;
    virtual void childrenChanged(Node*) override;
    virtual void childrenChanged(RenderObject*) override;
    virtual void childrenChanged(Widget*) override;
    virtual void checkedStateChanged(Node*) override;
    virtual void selectedChildrenChanged(Node*) override;

    virtual void remove(RenderObject*) override;
    virtual void remove(Node*) override;
    virtual void remove(Widget*) override;
    virtual void remove(AbstractInlineTextBox*) override;

    virtual const Element* rootAXEditableElement(const Node*) override;
    virtual bool nodeIsTextControl(const Node*) override;

    // Called by a node when text or a text equivalent (e.g. alt) attribute is changed.
    virtual void textChanged(Node*) override;
    virtual void textChanged(RenderObject*) override;
    // Called when a node has just been attached, so we can make sure we have the right subclass of AXObject.
    virtual void updateCacheAfterNodeIsAttached(Node*) override;

    virtual void handleAttributeChanged(const QualifiedName& attrName, Element*) override;
    virtual void handleFocusedUIElementChanged(Node* oldFocusedNode, Node* newFocusedNode) override;

    virtual void setCanvasObjectBounds(Element*, const LayoutRect&) override;

    virtual void clearWeakMembers(Visitor*) override;

    virtual void inlineTextBoxesUpdated(RenderObject* renderer) override;

    // Called when the scroll offset changes.
    virtual void handleScrollPositionChanged(FrameView*) override;
    virtual void handleScrollPositionChanged(RenderObject*) override;

    // Called when scroll bars are added / removed (as the view resizes).
    void handleScrollbarUpdate(FrameView*);
    void handleLayoutComplete(RenderObject*);
    void handleScrolledToAnchor(const Node* anchorNode);

    // FIXME(dmazzoni): in a following changelist we want to move these out of AXObjectCache
    // and stop calling them from outside of the accessibility directory.
    virtual AXObject* getOrCreate(RenderObject*) override;
    virtual AXObject* getOrCreate(Widget*) override;
    virtual AXObject* getOrCreate(Node*) override;
    virtual AXObject* getOrCreate(AbstractInlineTextBox*) override;
    virtual AXObject* get(RenderObject*) override;
    virtual void postNotification(RenderObject*, AXNotification, bool postToElement, PostType = PostAsynchronously) override;
    virtual void postNotification(Node*, AXNotification, bool postToElement, PostType = PostAsynchronously) override;
    virtual void postNotification(AXObject*, Document*, AXNotification, bool postToElement, PostType = PostAsynchronously) override;

    // Returns the root object for the entire document.
    AXObject* rootObject();

    // used for objects without backing elements
    AXObject* getOrCreate(AccessibilityRole);

    // will only return the AXObject if it already exists
    AXObject* get(Widget*);
    AXObject* get(Node*);
    AXObject* get(AbstractInlineTextBox*);

    void remove(AXID);

    void detachWrapper(AXObject*);
    void attachWrapper(AXObject*);
    void childrenChanged(AXObject*);
    void selectedChildrenChanged(RenderObject*);

    void handleActiveDescendantChanged(Node*);
    void handleAriaRoleChanged(Node*);
    void handleAriaExpandedChange(Node*);

    void recomputeIsIgnored(RenderObject* renderer);

    bool accessibilityEnabled();
    bool inlineTextBoxAccessibilityEnabled();

    void removeAXID(AXObject*);
    bool isIDinUse(AXID id) const { return m_idsInUse.contains(id); }

    AXID platformGenerateAXID() const;

    bool nodeHasRole(Node*, const AtomicString& role);

    AXComputedObjectAttributeCache* computedObjectAttributeCache() { return m_computedObjectAttributeCache.get(); }

protected:
    void postPlatformNotification(AXObject*, AXNotification);
    void textChanged(AXObject*);
    void labelChanged(Element*);

    // This is a weak reference cache for knowing if Nodes used by TextMarkers are valid.
    void setNodeInUse(Node* n) { m_textMarkerNodes.add(n); }
    void removeNodeForUse(Node* n) { m_textMarkerNodes.remove(n); }
    bool isNodeInUse(Node* n) { return m_textMarkerNodes.contains(n); }

private:
    Document& m_document;
    HashMap<AXID, RefPtr<AXObject> > m_objects;
    HashMap<RenderObject*, AXID> m_renderObjectMapping;
    HashMap<Widget*, AXID> m_widgetObjectMapping;
    HashMap<Node*, AXID> m_nodeObjectMapping;
    HashMap<AbstractInlineTextBox*, AXID> m_inlineTextBoxObjectMapping;
    HashSet<Node*> m_textMarkerNodes;
    OwnPtr<AXComputedObjectAttributeCache> m_computedObjectAttributeCache;

    HashSet<AXID> m_idsInUse;

    Timer<AXObjectCacheImpl> m_notificationPostTimer;
    Vector<pair<RefPtr<AXObject>, AXNotification> > m_notificationsToPost;
    void notificationPostTimerFired(Timer<AXObjectCacheImpl>*);

    static AXObject* focusedImageMapUIElement(HTMLAreaElement*);

    AXID getAXID(AXObject*);

    Settings* settings();
};

// This is the only subclass of AXObjectCache.
DEFINE_TYPE_CASTS(AXObjectCacheImpl, AXObjectCache, cache, true, true);

bool nodeHasRole(Node*, const String& role);
// This will let you know if aria-hidden was explicitly set to false.
bool isNodeAriaVisible(Node*);

}

#endif
