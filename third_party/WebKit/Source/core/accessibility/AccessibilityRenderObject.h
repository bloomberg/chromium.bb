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
#include <wtf/Forward.h>

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
    virtual RenderObject* renderer() const { return m_renderer; }
    virtual LayoutRect elementRect() const;

    // DEPRECATED - investigate and remove.
    virtual int layoutCount() const;

    void setRenderer(RenderObject*);
    RenderBoxModelObject* renderBoxModelObject() const;
    RenderView* topRenderer() const;
    Document* topDocument() const;
    HTMLLabelElement* labelElementContainer() const;
    bool shouldNotifyActiveDescendant() const;
    bool needsToUpdateChildren() const { return m_childrenDirty; }
    ScrollableArea* getScrollableAreaIfScrollable() const;
    virtual AccessibilityRole determineAccessibilityRole();

protected:
    RenderObject* m_renderer;
    mutable LayoutRect m_cachedElementRect;
    mutable LayoutRect m_cachedFrameRect;
    mutable IntPoint m_cachedScrollPosition;
    mutable bool m_cachedElementRectDirty;

    //
    // Overridden from AccessibilityObject.
    //

    virtual void init();
    virtual void detach();
    virtual bool isDetached() const { return !m_renderer; }
    virtual bool isAccessibilityRenderObject() const { return true; }

    // Check object role or purpose.
    virtual bool isAttachment() const;
    virtual bool isFileUploadButton() const;
    virtual bool isLinked() const;
    virtual bool isLoaded() const;
    virtual bool isOffScreen() const;
    virtual bool isReadOnly() const;
    virtual bool isUnvisited() const;
    virtual bool isVisited() const;

    // Check object state.
    virtual bool isFocused() const;
    virtual bool isSelected() const;

    // Check whether certain properties can be modified.
    virtual bool canSetTextRangeAttributes() const;
    virtual bool canSetValueAttribute() const;
    virtual bool canSetExpandedAttribute() const;

    // Rich text properties.
    virtual bool hasBoldFont() const;
    virtual bool hasItalicFont() const;
    virtual bool hasPlainText() const;
    virtual bool hasSameFont(RenderObject*) const;
    virtual bool hasSameFontColor(RenderObject*) const;
    virtual bool hasSameStyle(RenderObject*) const;
    virtual bool hasUnderline() const;

    // Whether objects are ignored, i.e. not included in the tree.
    virtual AccessibilityObjectInclusion defaultObjectInclusion() const;
    virtual bool computeAccessibilityIsIgnored() const;

    // Properties of static elements.
    virtual const AtomicString& accessKey() const;
    virtual AccessibilityObject* correspondingControlForLabelElement() const;
    virtual AccessibilityObject* correspondingLabelForControlElement() const;
    virtual bool exposesTitleUIElement() const;
    virtual void linkedUIElements(AccessibilityChildrenVector&) const;
    virtual AccessibilityOrientation orientation() const;
    virtual void tabChildren(AccessibilityChildrenVector&);
    virtual String text() const;
    virtual int textLength() const;
    virtual AccessibilityObject* titleUIElement() const;
    virtual KURL url() const;
    virtual void visibleChildren(AccessibilityChildrenVector&);

    // Properties of interactive elements.
    virtual const String& actionVerb() const;
    virtual void selectedChildren(AccessibilityChildrenVector&);
    virtual String stringValue() const;

    // ARIA attributes.
    virtual AccessibilityObject* activeDescendant() const;
    virtual void ariaFlowToElements(AccessibilityChildrenVector&) const;
    virtual bool ariaHasPopup() const;
    virtual void ariaOwnsElements(AccessibilityChildrenVector&) const;
    virtual bool ariaRoleHasPresentationalChildren() const;
    virtual void determineARIADropEffects(Vector<String>&);
    virtual bool isARIAGrabbed();
    virtual bool isPresentationalChildOfAriaRole() const;
    virtual bool shouldFocusActiveDescendant() const;
    virtual bool supportsARIADragging() const;
    virtual bool supportsARIADropping() const;
    virtual bool supportsARIAFlowTo() const;
    virtual bool supportsARIAOwns() const;

    // ARIA live-region features.
    virtual const AtomicString& ariaLiveRegionStatus() const;
    virtual const AtomicString& ariaLiveRegionRelevant() const;
    virtual bool ariaLiveRegionAtomic() const;
    virtual bool ariaLiveRegionBusy() const;

    // Accessibility Text.
    virtual String textUnderElement() const;

    // Accessibility Text - (To be deprecated).
    virtual String helpText() const;

    // Location and click point in frame-relative coordinates.
    virtual void checkCachedElementRect() const;
    virtual void updateCachedElementRect() const;
    virtual void markCachedElementRectDirty() const;
    virtual IntPoint clickPoint();

    // Hit testing.
    virtual AccessibilityObject* accessibilityHitTest(const IntPoint&) const;
    virtual AccessibilityObject* elementAccessibilityHitTest(const IntPoint&) const;

    // High-level accessibility tree access. Other modules should only use these functions.
    virtual AccessibilityObject* parentObject() const;
    virtual AccessibilityObject* parentObjectIfExists() const;

    // Low-level accessibility tree exploration, only for use within the accessibility module.
    virtual AccessibilityObject* firstChild() const;
    virtual AccessibilityObject* lastChild() const;
    virtual AccessibilityObject* previousSibling() const;
    virtual AccessibilityObject* nextSibling() const;
    virtual void addChildren();
    virtual bool canHaveChildren() const;
    virtual void updateChildrenIfNecessary();
    virtual void setNeedsToUpdateChildren() { m_childrenDirty = true; }
    virtual void clearChildren();
    virtual AccessibilityObject* observableObject() const;

    // Properties of the object's owning document or page.
    virtual double estimatedLoadingProgress() const;

    // DOM and Render tree access.
    virtual Node* node() const;
    virtual Document* document() const;
    virtual FrameView* documentFrameView() const;
    virtual Element* anchorElement() const;
    virtual Widget* widget() const;
    virtual Widget* widgetForAttachmentView() const;

    // Selected text.
    virtual PlainTextRange selectedTextRange() const;
    virtual VisibleSelection selection() const;
    virtual String selectedText() const;

    // Modify or take an action on an object.
    virtual void setFocused(bool);
    virtual void setSelectedTextRange(const PlainTextRange&);
    virtual void setValue(const String&);
    virtual void setSelectedRows(AccessibilityChildrenVector&);
    virtual void scrollTo(const IntPoint&) const;

    // Notifications that this object may have changed.
    virtual void handleActiveDescendantChanged();
    virtual void handleAriaExpandedChanged();
    virtual void textChanged();

    // Text metrics. Most of these should be deprecated, needs major cleanup.
    virtual VisiblePositionRange visiblePositionRange() const;
    virtual VisiblePositionRange visiblePositionRangeForLine(unsigned) const;
    virtual IntRect boundsForVisiblePositionRange(const VisiblePositionRange&) const;
    virtual void setSelectedVisiblePositionRange(const VisiblePositionRange&) const;
    virtual VisiblePosition visiblePositionForPoint(const IntPoint&) const;
    virtual VisiblePosition visiblePositionForIndex(unsigned indexValue, bool lastIndexOK) const;
    virtual int index(const VisiblePosition&) const;
    virtual VisiblePosition visiblePositionForIndex(int) const;
    virtual int indexForVisiblePosition(const VisiblePosition&) const;
    virtual void lineBreaks(Vector<int>&) const;
    virtual PlainTextRange doAXRangeForLine(unsigned) const;
    virtual PlainTextRange doAXRangeForIndex(unsigned) const;
    virtual String doAXStringForRange(const PlainTextRange&) const;
    virtual IntRect doAXBoundsForRange(const PlainTextRange&) const;

    // CSS3 Speech properties.
    virtual ESpeak speakProperty() const;

private:
    bool isAllowedChildOfTree() const;
    bool hasTextAlternative() const;
    void ariaListboxSelectedChildren(AccessibilityChildrenVector&);
    void ariaListboxVisibleChildren(AccessibilityChildrenVector&);
    PlainTextRange ariaSelectedTextRange() const;
    Element* rootEditableElementForPosition(const Position&) const;
    bool nodeIsTextControl(const Node*) const;
    bool isTabItemSelected() const;
    void addRadioButtonGroupMembers(AccessibilityChildrenVector& linkedUIElements) const;
    AccessibilityObject* internalLinkElement() const;
    AccessibilityObject* accessibilityImageMapHitTest(HTMLAreaElement*, const IntPoint&) const;
    AccessibilityObject* accessibilityParentForImageMap(HTMLMapElement*) const;
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
