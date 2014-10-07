/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Apple Inc. All rights reserved.
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
 *
 */

#ifndef HTMLLabelElement_h
#define HTMLLabelElement_h

#include "core/html/FormAssociatedElement.h"
#include "core/html/HTMLElement.h"
#include "core/html/LabelableElement.h"

namespace blink {

class HTMLLabelElement final : public HTMLElement, public FormAssociatedElement {
    DEFINE_WRAPPERTYPEINFO();
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(HTMLLabelElement);
public:
    static PassRefPtrWillBeRawPtr<HTMLLabelElement> create(Document&, HTMLFormElement*);
    LabelableElement* control() const;

    virtual bool willRespondToMouseClickEvents() override;

    virtual void trace(Visitor*) override;

    virtual HTMLFormElement* formOwner() const override;


#if !ENABLE(OILPAN)
    using Node::ref;
    using Node::deref;
#endif

private:
    explicit HTMLLabelElement(Document&, HTMLFormElement*);
    bool isInInteractiveContent(Node*) const;

    virtual bool rendererIsFocusable() const override;
    virtual bool isInteractiveContent() const override;
    virtual void accessKeyAction(bool sendMouseEvents) override;

    virtual void attributeWillChange(const QualifiedName&, const AtomicString& oldValue, const AtomicString& newValue) override;
    virtual InsertionNotificationRequest insertedInto(ContainerNode*) override;
    virtual void removedFrom(ContainerNode*) override;

    // Overridden to update the hover/active state of the corresponding control.
    virtual void setActive(bool = true) override;
    virtual void setHovered(bool = true) override;

    // Overridden to either click() or focus() the corresponding control.
    virtual void defaultEventHandler(Event*) override;

    virtual void focus(bool restorePreviousSelection, FocusType) override;

    // FormAssociatedElement methods
    virtual bool isFormControlElement() const override { return false; }
    virtual bool isEnumeratable() const override { return false; }
    virtual bool isLabelElement() const override { return true; }
#if !ENABLE(OILPAN)
    virtual void refFormAssociatedElement() override { ref(); }
    virtual void derefFormAssociatedElement() override { deref(); }
#endif

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) override;

    void updateLabel(TreeScope&, const AtomicString& oldForAttributeValue, const AtomicString& newForAttributeValue);

    bool m_processingClick;
};


template<typename T> inline const T& toElement(const FormAssociatedElement&);
template<typename T> inline const T* toElement(const FormAssociatedElement*);
// Make toHTMLLabelElement() accept a FormAssociatedElement as input instead of a Node.
template<> inline const HTMLLabelElement* toElement<HTMLLabelElement>(const FormAssociatedElement* element)
{
    const HTMLLabelElement* labelElement = static_cast<const HTMLLabelElement*>(element);
    // FormAssociatedElement doesn't have hasTagName, hence check for assert.
    ASSERT_WITH_SECURITY_IMPLICATION(!labelElement || labelElement->hasTagName(HTMLNames::labelTag));
    return labelElement;
}

template<> inline const HTMLLabelElement& toElement<HTMLLabelElement>(const FormAssociatedElement& element)
{
    const HTMLLabelElement& labelElement = static_cast<const HTMLLabelElement&>(element);
    // FormAssociatedElement doesn't have hasTagName, hence check for assert.
    ASSERT_WITH_SECURITY_IMPLICATION(labelElement.hasTagName(HTMLNames::labelTag));
    return labelElement;
}

} // namespace blink

#endif // HTMLLabelElement_h
