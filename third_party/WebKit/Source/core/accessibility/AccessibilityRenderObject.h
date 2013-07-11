/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef AccessibilityRenderObject_h
#define AccessibilityRenderObject_h

#include "core/accessibility/AccessibilityNodeObject.h"
#include "core/platform/graphics/LayoutRect.h"
#include "wtf/Forward.h"

namespace WebCore {

class AccessibilitySVGRoot;
class AXObjectCache;
class Element;
class Frame;
class FrameView;
class HitTestResult;
class HTMLAnchorElement;
class HTMLAreaElement;
class HTMLElement;
class HTMLLabelElement;
class HTMLMapElement;
class HTMLSelectElement;
class IntPoint;
class IntSize;
class Node;
class RenderListBox;
class RenderTextControl;
class RenderView;
class VisibleSelection;
class Widget;

class AccessibilityRenderObject : public AccessibilityNodeObject {
protected:
    explicit AccessibilityRenderObject(RenderObject*);
public:
    static PassRefPtr<AccessibilityRenderObject> create(RenderObject*);
    virtual ~AccessibilityRenderObject();

    // Public, overridden from AccessibilityObject.
    virtual RenderObject* renderer() const OVERRIDE { return m_renderer; }
    virtual LayoutRect elementRect() const OVERRIDE;

    void setRenderer(RenderObject*);
    RenderBoxModelObject* renderBoxModelObject() const;
    RenderView* topRenderer() const;
    Document* topDocument() const;
    HTMLLabelElement* labelElementContainer() const;
    bool shouldNotifyActiveDescendant() const;
    bool needsToUpdateChildren() const { return m_childrenDirty; }
    ScrollableArea* getScrollableAreaIfScrollable() const;
    virtual AccessibilityRole determineAccessibilityRole() OVERRIDE;
    void checkCachedElementRect() const;
    void updateCachedElementRect() const;

protected:
    RenderObject* m_renderer;
    mutable LayoutRect m_cachedElementRect;
    mutable LayoutRect m_cachedFrameRect;
    mutable IntPoint m_cachedScrollPosition;
    mutable bool m_cachedElementRectDirty;

    //
    // Overridden from AccessibilityObject.
    //

    virtual void init() OVERRIDE;
    virtual void detach() OVERRIDE;
    virtual bool isDetached() const OVERRIDE { return !m_renderer; }
    virtual bool isAccessibilityRenderObject() const OVERRIDE { return true; }

    // Check object role or purpose.
    virtual bool isAttachment() const OVERRIDE;
    virtual bool isFileUploadButton() const OVERRIDE;
    virtual bool isLinked() const OVERRIDE;
    virtual bool isLoaded() const OVERRIDE;
    virtual bool isOffScreen() const OVERRIDE;
    virtual bool isReadOnly() const OVERRIDE;
    virtual bool isVisited() const OVERRIDE;

    // Check object state.
    virtual bool isFocused() const OVERRIDE;
    virtual bool isSelected() const OVERRIDE;

    // Check whether certain properties can be modified.
    virtual bool canSetValueAttribute() const OVERRIDE;

    // Whether objects are ignored, i.e. not included in the tree.
    virtual AccessibilityObjectInclusion defaultObjectInclusion() const OVERRIDE;
    virtual bool computeAccessibilityIsIgnored() const OVERRIDE;

    // Properties of static elements.
    virtual const AtomicString& accessKey() const OVERRIDE;
    virtual bool exposesTitleUIElement() const OVERRIDE;
    virtual AccessibilityOrientation orientation() const OVERRIDE;
    virtual String text() const OVERRIDE;
    virtual int textLength() const OVERRIDE;
    virtual AccessibilityObject* titleUIElement() const OVERRIDE;
    virtual KURL url() const OVERRIDE;

    // Properties of interactive elements.
    virtual String actionVerb() const OVERRIDE;
    virtual void selectedChildren(AccessibilityChildrenVector&) OVERRIDE;
    virtual String stringValue() const OVERRIDE;

    // ARIA attributes.
    virtual AccessibilityObject* activeDescendant() const OVERRIDE;
    virtual void ariaFlowToElements(AccessibilityChildrenVector&) const OVERRIDE;
    virtual bool ariaHasPopup() const OVERRIDE;
    virtual bool ariaRoleHasPresentationalChildren() const OVERRIDE;
    virtual bool isPresentationalChildOfAriaRole() const OVERRIDE;
    virtual bool shouldFocusActiveDescendant() const OVERRIDE;
    virtual bool supportsARIADragging() const OVERRIDE;
    virtual bool supportsARIADropping() const OVERRIDE;
    virtual bool supportsARIAFlowTo() const OVERRIDE;
    virtual bool supportsARIAOwns() const OVERRIDE;

    // ARIA live-region features.
    virtual const AtomicString& ariaLiveRegionStatus() const OVERRIDE;
    virtual const AtomicString& ariaLiveRegionRelevant() const OVERRIDE;
    virtual bool ariaLiveRegionAtomic() const OVERRIDE;
    virtual bool ariaLiveRegionBusy() const OVERRIDE;

    // Accessibility Text.
    virtual String textUnderElement() const OVERRIDE;

    // Accessibility Text - (To be deprecated).
    virtual String helpText() const OVERRIDE;

    // Location and click point in frame-relative coordinates.
    virtual void markCachedElementRectDirty() const OVERRIDE;
    virtual IntPoint clickPoint() OVERRIDE;

    // Hit testing.
    virtual AccessibilityObject* accessibilityHitTest(const IntPoint&) const OVERRIDE;
    virtual AccessibilityObject* elementAccessibilityHitTest(const IntPoint&) const OVERRIDE;

    // High-level accessibility tree access. Other modules should only use these functions.
    virtual AccessibilityObject* parentObject() const OVERRIDE;
    virtual AccessibilityObject* parentObjectIfExists() const OVERRIDE;

    // Low-level accessibility tree exploration, only for use within the accessibility module.
    virtual AccessibilityObject* firstChild() const OVERRIDE;
    virtual AccessibilityObject* nextSibling() const OVERRIDE;
    virtual void addChildren() OVERRIDE;
    virtual bool canHaveChildren() const OVERRIDE;
    virtual void updateChildrenIfNecessary() OVERRIDE;
    virtual void setNeedsToUpdateChildren() OVERRIDE { m_childrenDirty = true; }
    virtual void clearChildren() OVERRIDE;
    virtual AccessibilityObject* observableObject() const OVERRIDE;

    // Properties of the object's owning document or page.
    virtual double estimatedLoadingProgress() const OVERRIDE;

    // DOM and Render tree access.
    virtual Node* node() const OVERRIDE;
    virtual Document* document() const OVERRIDE;
    virtual FrameView* documentFrameView() const OVERRIDE;
    virtual Element* anchorElement() const OVERRIDE;
    virtual Widget* widgetForAttachmentView() const OVERRIDE;

    // Selected text.
    virtual PlainTextRange selectedTextRange() const OVERRIDE;
    virtual String selectedText() const OVERRIDE;

    // Modify or take an action on an object.
    virtual void setFocused(bool) OVERRIDE;
    virtual void setSelectedTextRange(const PlainTextRange&) OVERRIDE;
    virtual void setValue(const String&) OVERRIDE;
    virtual void scrollTo(const IntPoint&) const OVERRIDE;

    // Notifications that this object may have changed.
    virtual void handleActiveDescendantChanged() OVERRIDE;
    virtual void handleAriaExpandedChanged() OVERRIDE;

    // Text metrics. Most of these should be deprecated, needs major cleanup.
    virtual int index(const VisiblePosition&) const OVERRIDE;
    virtual VisiblePosition visiblePositionForIndex(int) const OVERRIDE;
    virtual void lineBreaks(Vector<int>&) const OVERRIDE;

private:
    bool isAllowedChildOfTree() const;
    bool hasTextAlternative() const;
    void ariaListboxSelectedChildren(AccessibilityChildrenVector&);
    PlainTextRange ariaSelectedTextRange() const;
    bool nodeIsTextControl(const Node*) const;
    bool isTabItemSelected() const;
    AccessibilityObject* internalLinkElement() const;
    AccessibilityObject* accessibilityImageMapHitTest(HTMLAreaElement*, const IntPoint&) const;
    bool renderObjectIsObservable(RenderObject*) const;
    RenderObject* renderParentObject() const;
    bool isDescendantOfElementType(const QualifiedName& tagName) const;
    bool isSVGImage() const;
    void detachRemoteSVGRoot();
    AccessibilitySVGRoot* remoteSVGRootElement() const;
    AccessibilityObject* remoteSVGElementHitTest(const IntPoint&) const;
    void offsetBoundingBoxForRemoteSVGElement(LayoutRect&) const;
    void addHiddenChildren();
    void addTextFieldChildren();
    void addImageMapChildren();
    void addCanvasChildren();
    void addAttachmentChildren();
    void addRemoteSVGChildren();
    void ariaSelectedRows(AccessibilityChildrenVector&);
    bool elementAttributeValue(const QualifiedName&) const;
    bool inheritsPresentationalRole() const;
    LayoutRect computeElementRect() const;
    VisibleSelection selection() const;
    String stringForRange(const PlainTextRange&) const;
    AccessibilityObject* correspondingControlForLabelElement() const;
    int indexForVisiblePosition(const VisiblePosition&) const;
};

inline AccessibilityRenderObject* toAccessibilityRenderObject(AccessibilityObject* object)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!object || object->isAccessibilityRenderObject());
    return static_cast<AccessibilityRenderObject*>(object);
}

inline const AccessibilityRenderObject* toAccessibilityRenderObject(const AccessibilityObject* object)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!object || object->isAccessibilityRenderObject());
    return static_cast<const AccessibilityRenderObject*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toAccessibilityRenderObject(const AccessibilityRenderObject*);

} // namespace WebCore

#endif // AccessibilityRenderObject_h
