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
#include <wtf/Forward.h>

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

    virtual bool computeAccessibilityIsIgnored() const;
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
    Element* menuElementForMenuButton() const;
    Element* menuItemElementForMenu() const;
    Element* mouseButtonListener() const;
    AccessibilityRole remapAriaRoleDueToParent(AccessibilityRole) const;

    //
    // Overridden from AccessibilityObject.
    //

    virtual void init();
    virtual void detach();
    virtual bool isDetached() const { return !m_node; }
    virtual bool isAccessibilityNodeObject() const { return true; }

    // Check object role or purpose.
    virtual bool isAnchor() const;
    virtual bool isControl() const;
    virtual bool isFieldset() const;
    virtual bool isGroup() const;
    virtual bool isHeading() const;
    virtual bool isHovered() const;
    virtual bool isImage() const;
    virtual bool isImageButton() const;
    virtual bool isInputImage() const;
    virtual bool isLink() const;
    virtual bool isMenu() const;
    virtual bool isMenuBar() const;
    virtual bool isMenuButton() const;
    virtual bool isMenuItem() const;
    virtual bool isMenuRelated() const;
    virtual bool isMultiSelectable() const;
    virtual bool isNativeCheckboxOrRadio() const;
    virtual bool isNativeImage() const;
    virtual bool isNativeTextControl() const;
    virtual bool isPasswordField() const;
    virtual bool isProgressIndicator() const;
    virtual bool isSearchField() const;
    virtual bool isSlider() const;

    // Check object state.
    virtual bool isChecked() const;
    virtual bool isEnabled() const;
    virtual bool isIndeterminate() const;
    virtual bool isPressed() const;
    virtual bool isReadOnly() const;
    virtual bool isRequired() const;

    // Check whether certain properties can be modified.
    virtual bool canSetFocusAttribute() const;

    // Properties of static elements.
    virtual bool canvasHasFallbackContent() const;
    virtual int headingLevel() const;
    virtual unsigned hierarchicalLevel() const;
    virtual String text() const;

    // Properties of interactive elements.
    virtual AccessibilityButtonState checkboxOrRadioValue() const;
    virtual void colorValue(int& r, int& g, int& b) const;
    virtual String valueDescription() const;
    virtual float valueForRange() const;
    virtual float maxValueForRange() const;
    virtual float minValueForRange() const;
    virtual AccessibilityObject* selectedRadioButton();
    virtual AccessibilityObject* selectedTabItem();
    virtual float stepValueForRange() const;
    virtual String stringValue() const;

    // ARIA attributes.
    String ariaDescribedByAttribute() const;
    virtual String ariaLabeledByAttribute() const;
    AccessibilityRole ariaRoleAttribute() const;

    // Accessibility Text.
    virtual void accessibilityText(Vector<AccessibilityText>&);
    virtual String textUnderElement() const;

    // Accessibility Text - (To be deprecated).
    virtual String accessibilityDescription() const;
    virtual String title() const;
    virtual String helpText() const;

    // Location and click point in frame-relative coordinates.
    virtual LayoutRect elementRect() const;

    // High-level accessibility tree access.
    virtual AccessibilityObject* parentObject() const;
    virtual AccessibilityObject* parentObjectIfExists() const;

    // Low-level accessibility tree exploration.
    virtual AccessibilityObject* firstChild() const;
    virtual AccessibilityObject* lastChild() const;
    virtual AccessibilityObject* previousSibling() const;
    virtual AccessibilityObject* nextSibling() const;
    virtual void addChildren();
    virtual void addChild(AccessibilityObject*);
    virtual void insertChild(AccessibilityObject*, unsigned index);
    virtual bool canHaveChildren() const;

    // DOM and Render tree access.
    virtual Element* actionElement() const;
    virtual Element* anchorElement() const;
    virtual Document* document() const;
    virtual Node* node() const { return m_node; }
    void setNode(Node*);

    // Modify or take an action on an object.
    virtual void increment();
    virtual void decrement();

    // Notifications that this object may have changed.
    virtual void childrenChanged();
    virtual void updateAccessibilityRole();

private:
    Node* m_node;

    String alternativeTextForWebArea() const;
    void alternativeText(Vector<AccessibilityText>&) const;
    void ariaLabeledByText(Vector<AccessibilityText>&) const;
    void changeValueByPercent(float percentChange);
    void helpText(Vector<AccessibilityText>&) const;
    void titleElementText(Vector<AccessibilityText>&);
    void visibleText(Vector<AccessibilityText>&) const;
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
