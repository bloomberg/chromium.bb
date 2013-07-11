/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
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

#ifndef AccessibilityNodeObject_h
#define AccessibilityNodeObject_h

#include "core/accessibility/AccessibilityObject.h"
#include "core/platform/graphics/LayoutRect.h"
#include "wtf/Forward.h"

namespace WebCore {

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

class AccessibilityNodeObject : public AccessibilityObject {
protected:
    explicit AccessibilityNodeObject(Node*);

public:
    static PassRefPtr<AccessibilityNodeObject> create(Node*);
    virtual ~AccessibilityNodeObject();

protected:
    // Protected data.
    AccessibilityRole m_ariaRole;
    bool m_childrenDirty;
#ifndef NDEBUG
    bool m_initialized;
#endif

    virtual bool computeAccessibilityIsIgnored() const OVERRIDE;
    virtual AccessibilityRole determineAccessibilityRole();

    String accessibilityDescriptionForElements(Vector<Element*> &elements) const;
    void alterSliderValue(bool increase);
    String ariaAccessibilityDescription() const;
    void ariaLabeledByElements(Vector<Element*>& elements) const;
    void changeValueByStep(bool increase);
    AccessibilityRole determineAriaRoleAttribute() const;
    void elementsFromAttribute(Vector<Element*>& elements, const QualifiedName&) const;
    bool hasContentEditableAttributeSet() const;
    bool isARIARange() const;
    bool isDescendantOfBarrenParent() const;
    // This returns true if it's focusable but it's not content editable and it's not a control or ARIA control.
    bool isGenericFocusableElement() const;
    HTMLLabelElement* labelForElement(Element*) const;
    AccessibilityObject* menuButtonForMenu() const;
    Element* menuItemElementForMenu() const;
    Element* mouseButtonListener() const;
    AccessibilityRole remapAriaRoleDueToParent(AccessibilityRole) const;
    bool isNativeCheckboxOrRadio() const;
    void setNode(Node*);

    //
    // Overridden from AccessibilityObject.
    //

    virtual void init() OVERRIDE;
    virtual void detach() OVERRIDE;
    virtual bool isDetached() const OVERRIDE { return !m_node; }
    virtual bool isAccessibilityNodeObject() const OVERRIDE { return true; }

    // Check object role or purpose.
    virtual bool isAnchor() const OVERRIDE;
    virtual bool isControl() const OVERRIDE;
    virtual bool isFieldset() const OVERRIDE;
    virtual bool isHeading() const OVERRIDE;
    virtual bool isHovered() const OVERRIDE;
    virtual bool isImage() const OVERRIDE;
    bool isImageButton() const;
    virtual bool isInputImage() const OVERRIDE;
    virtual bool isLink() const;
    virtual bool isMenu() const OVERRIDE;
    virtual bool isMenuButton() const OVERRIDE;
    virtual bool isMultiSelectable() const OVERRIDE;
    bool isNativeImage() const;
    virtual bool isNativeTextControl() const OVERRIDE;
    virtual bool isNonNativeTextControl() const OVERRIDE;
    virtual bool isPasswordField() const OVERRIDE;
    virtual bool isProgressIndicator() const OVERRIDE;
    virtual bool isSlider() const OVERRIDE;

    // Check object state.
    virtual bool isChecked() const OVERRIDE;
    virtual bool isEnabled() const OVERRIDE;
    virtual bool isIndeterminate() const OVERRIDE;
    virtual bool isPressed() const OVERRIDE;
    virtual bool isReadOnly() const OVERRIDE;
    virtual bool isRequired() const OVERRIDE;

    // Check whether certain properties can be modified.
    virtual bool canSetFocusAttribute() const OVERRIDE;

    // Properties of static elements.
    virtual bool canvasHasFallbackContent() const OVERRIDE;
    virtual int headingLevel() const OVERRIDE;
    virtual unsigned hierarchicalLevel() const OVERRIDE;
    virtual String text() const OVERRIDE;

    // Properties of interactive elements.
    virtual AccessibilityButtonState checkboxOrRadioValue() const OVERRIDE;
    virtual void colorValue(int& r, int& g, int& b) const OVERRIDE;
    virtual String valueDescription() const OVERRIDE;
    virtual float valueForRange() const OVERRIDE;
    virtual float maxValueForRange() const OVERRIDE;
    virtual float minValueForRange() const OVERRIDE;
    virtual String stringValue() const OVERRIDE;

    // ARIA attributes.
    virtual String ariaDescribedByAttribute() const;
    virtual String ariaLabeledByAttribute() const OVERRIDE;
    virtual AccessibilityRole ariaRoleAttribute() const OVERRIDE;

    // Accessibility Text.
    virtual void accessibilityText(Vector<AccessibilityText>&) OVERRIDE;
    virtual String textUnderElement() const OVERRIDE;

    // Accessibility Text - (To be deprecated).
    virtual String accessibilityDescription() const OVERRIDE;
    virtual String title() const OVERRIDE;
    virtual String helpText() const OVERRIDE;

    // Location and click point in frame-relative coordinates.
    virtual LayoutRect elementRect() const OVERRIDE;

    // High-level accessibility tree access.
    virtual AccessibilityObject* parentObject() const OVERRIDE;
    virtual AccessibilityObject* parentObjectIfExists() const OVERRIDE;

    // Low-level accessibility tree exploration.
    virtual AccessibilityObject* firstChild() const OVERRIDE;
    virtual AccessibilityObject* nextSibling() const OVERRIDE;
    virtual void addChildren() OVERRIDE;
    virtual bool canHaveChildren() const OVERRIDE;
    void addChild(AccessibilityObject*);
    void insertChild(AccessibilityObject*, unsigned index);

    // DOM and Render tree access.
    virtual Element* actionElement() const OVERRIDE;
    virtual Element* anchorElement() const OVERRIDE;
    virtual Document* document() const OVERRIDE;
    virtual Node* node() const OVERRIDE { return m_node; }

    // Modify or take an action on an object.
    virtual void increment() OVERRIDE;
    virtual void decrement() OVERRIDE;

    // Notifications that this object may have changed.
    virtual void childrenChanged() OVERRIDE;
    virtual void selectionChanged() OVERRIDE;
    virtual void textChanged() OVERRIDE;
    virtual void updateAccessibilityRole() OVERRIDE;

private:
    Node* m_node;

    String alternativeTextForWebArea() const;
    void alternativeText(Vector<AccessibilityText>&) const;
    void ariaLabeledByText(Vector<AccessibilityText>&) const;
    void changeValueByPercent(float percentChange);
    void helpText(Vector<AccessibilityText>&) const;
    void titleElementText(Vector<AccessibilityText>&);
    void visibleText(Vector<AccessibilityText>&) const;
    float stepValueForRange() const;
};

inline AccessibilityNodeObject* toAccessibilityNodeObject(AccessibilityObject* object)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!object || object->isAccessibilityNodeObject());
    return static_cast<AccessibilityNodeObject*>(object);
}

inline const AccessibilityNodeObject* toAccessibilityNodeObject(const AccessibilityObject* object)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!object || object->isAccessibilityNodeObject());
    return static_cast<const AccessibilityNodeObject*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toAccessibilityNodeObject(const AccessibilityNodeObject*);

} // namespace WebCore

#endif // AccessibilityNodeObject_h
