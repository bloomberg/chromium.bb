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

#ifndef AXLayoutObject_h
#define AXLayoutObject_h

#include "modules/ModulesExport.h"
#include "modules/accessibility/AXNodeObject.h"
#include "platform/geometry/LayoutRect.h"
#include "wtf/Forward.h"

namespace blink {

class AXObjectCacheImpl;
class AXSVGRoot;
class Element;
class FrameView;
class HTMLAreaElement;
class IntPoint;
class Node;
class VisibleSelection;
class Widget;

class MODULES_EXPORT AXLayoutObject : public AXNodeObject {
protected:
    AXLayoutObject(LayoutObject*, AXObjectCacheImpl&);

public:
    static PassRefPtr<AXLayoutObject> create(LayoutObject*, AXObjectCacheImpl&);
    virtual ~AXLayoutObject();

    // Public, overridden from AXObject.
    virtual LayoutObject* layoutObject() const override final { return m_layoutObject; }
    virtual LayoutRect elementRect() const override;

    void setLayoutObject(LayoutObject*);
    LayoutBoxModelObject* layoutBoxModelObject() const;
    Document* topDocument() const;
    bool shouldNotifyActiveDescendant() const;
    virtual ScrollableArea* getScrollableAreaIfScrollable() const override final;
    virtual AccessibilityRole determineAccessibilityRole() override;
    void checkCachedElementRect() const;
    void updateCachedElementRect() const;

protected:
    LayoutObject* m_layoutObject;
    mutable LayoutRect m_cachedElementRect;
    mutable LayoutRect m_cachedFrameRect;
    mutable IntPoint m_cachedScrollPosition;
    mutable bool m_cachedElementRectDirty;

    //
    // Overridden from AXObject.
    //

    virtual void init() override;
    virtual void detach() override;
    virtual bool isDetached() const override { return !m_layoutObject; }
    virtual bool isAXLayoutObject() const override { return true; }

    // Check object role or purpose.
    virtual bool isAttachment() const override;
    virtual bool isRichlyEditable() const override;
    virtual bool isLinked() const override;
    virtual bool isLoaded() const override;
    virtual bool isOffScreen() const override;
    virtual bool isReadOnly() const override;
    virtual bool isVisited() const override;

    // Check object state.
    virtual bool isFocused() const override;
    virtual bool isSelected() const override;

    // Whether objects are ignored, i.e. not included in the tree.
    virtual AXObjectInclusion defaultObjectInclusion(IgnoredReasons* = nullptr) const override;
    virtual bool computeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const override;

    // Properties of static elements.
    virtual const AtomicString& accessKey() const override;
    virtual RGBA32 backgroundColor() const override final;
    virtual RGBA32 color() const override final;
    // Font size is in pixels.
    virtual float fontSize() const override final;
    virtual AccessibilityOrientation orientation() const override;
    virtual String text() const override;
    virtual AccessibilityTextDirection textDirection() const override final;
    virtual int textLength() const override;
    virtual TextStyle textStyle() const override final;
    virtual KURL url() const override;

    // Inline text boxes.
    virtual void loadInlineTextBoxes() override;
    virtual AXObject* nextOnLine() const override;
    virtual AXObject* previousOnLine() const override;

    // Properties of interactive elements.
    virtual String actionVerb() const override;
    virtual String stringValue() const override;

    // ARIA attributes.
    virtual AXObject* activeDescendant() const override;
    virtual void ariaFlowToElements(AccessibilityChildrenVector&) const override;
    virtual void ariaControlsElements(AccessibilityChildrenVector&) const override;
    virtual void deprecatedAriaDescribedbyElements(AccessibilityChildrenVector&) const override;
    virtual void deprecatedAriaLabelledbyElements(AccessibilityChildrenVector&) const override;
    virtual void ariaOwnsElements(AccessibilityChildrenVector&) const override;

    virtual bool ariaHasPopup() const override;
    virtual bool ariaRoleHasPresentationalChildren() const override;
    virtual AXObject* ancestorForWhichThisIsAPresentationalChild() const override;
    virtual bool shouldFocusActiveDescendant() const override;
    virtual bool supportsARIADragging() const override;
    virtual bool supportsARIADropping() const override;
    virtual bool supportsARIAFlowTo() const override;
    virtual bool supportsARIAOwns() const override;

    // ARIA live-region features.
    virtual const AtomicString& liveRegionStatus() const override;
    virtual const AtomicString& liveRegionRelevant() const override;
    virtual bool liveRegionAtomic() const override;
    virtual bool liveRegionBusy() const override;

    // Accessibility Text.
    virtual String deprecatedTextUnderElement(TextUnderElementMode) const override;

    // Accessibility Text - (To be deprecated).
    virtual String deprecatedHelpText() const override;

    // Location and click point in frame-relative coordinates.
    virtual void markCachedElementRectDirty() const override;
    virtual IntPoint clickPoint() override;

    // Hit testing.
    virtual AXObject* accessibilityHitTest(const IntPoint&) const override;
    virtual AXObject* elementAccessibilityHitTest(const IntPoint&) const override;

    // High-level accessibility tree access. Other modules should only use these functions.
    virtual AXObject* computeParent() const override;
    virtual AXObject* computeParentIfExists() const override;

    // Low-level accessibility tree exploration, only for use within the accessibility module.
    virtual AXObject* firstChild() const override;
    virtual AXObject* nextSibling() const override;
    virtual void addChildren() override;
    virtual bool canHaveChildren() const override;
    virtual void updateChildrenIfNecessary() override;
    virtual bool needsToUpdateChildren() const override { return m_childrenDirty; }
    virtual void setNeedsToUpdateChildren() override { m_childrenDirty = true; }
    virtual void clearChildren() override;

    // Properties of the object's owning document or page.
    virtual double estimatedLoadingProgress() const override;

    // DOM and layout tree access.
    virtual Node* node() const override;
    virtual Document* document() const override;
    virtual FrameView* documentFrameView() const override;
    virtual Element* anchorElement() const override;
    virtual Widget* widgetForAttachmentView() const override;

    // Selected text.
    virtual PlainTextRange selectedTextRange() const override;

    // Modify or take an action on an object.
    virtual void setSelectedTextRange(const PlainTextRange&) override;
    virtual void setValue(const String&) override;

    // Notifications that this object may have changed.
    virtual void handleActiveDescendantChanged() override;
    virtual void handleAriaExpandedChanged() override;
    virtual void textChanged() override;

    // Text metrics. Most of these should be deprecated, needs major cleanup.
    virtual int index(const VisiblePosition&) const override;
    virtual VisiblePosition visiblePositionForIndex(int) const override;
    virtual void lineBreaks(Vector<int>&) const override;

private:
    AXObject* treeAncestorDisallowingChild() const;
    void ariaListboxSelectedChildren(AccessibilityChildrenVector&);
    PlainTextRange visibleSelectionUnderObject() const;
    bool nodeIsTextControl(const Node*) const;
    bool isTabItemSelected() const;
    AXObject* accessibilityImageMapHitTest(HTMLAreaElement*, const IntPoint&) const;
    LayoutObject* layoutParentObject() const;
    bool isSVGImage() const;
    void detachRemoteSVGRoot();
    AXSVGRoot* remoteSVGRootElement() const;
    AXObject* remoteSVGElementHitTest(const IntPoint&) const;
    void offsetBoundingBoxForRemoteSVGElement(LayoutRect&) const;
    void addHiddenChildren();
    void addTextFieldChildren();
    void addImageMapChildren();
    void addCanvasChildren();
    void addAttachmentChildren();
    void addPopupChildren();
    void addRemoteSVGChildren();
    void addInlineTextBoxChildren(bool force);

    void ariaSelectedRows(AccessibilityChildrenVector&);
    bool elementAttributeValue(const QualifiedName&) const;
    LayoutRect computeElementRect() const;
    VisibleSelection selection() const;
    int indexForVisiblePosition(const VisiblePosition&) const;
};

DEFINE_AX_OBJECT_TYPE_CASTS(AXLayoutObject, isAXLayoutObject());

} // namespace blink

#endif // AXLayoutObject_h
