/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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

#include "config.h"
#include "core/html/HTMLFormControlElement.h"

#include "core/dom/Event.h"
#include "core/dom/EventNames.h"
#include "core/dom/PostAttachCallbacks.h"
#include "core/html/HTMLFieldSetElement.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLLegendElement.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/html/ValidityState.h"
#include "core/html/forms/ValidationMessage.h"
#include "core/page/UseCounter.h"
#include "core/rendering/RenderBox.h"
#include "core/rendering/RenderTheme.h"
#include "wtf/Vector.h"

namespace WebCore {

using namespace HTMLNames;
using namespace std;

HTMLFormControlElement::HTMLFormControlElement(const QualifiedName& tagName, Document& document, HTMLFormElement* form)
    : LabelableElement(tagName, document)
    , m_disabled(false)
    , m_isReadOnly(false)
    , m_isRequired(false)
    , m_valueMatchesRenderer(false)
    , m_ancestorDisabledState(AncestorDisabledStateUnknown)
    , m_dataListAncestorState(Unknown)
    , m_willValidateInitialized(false)
    , m_willValidate(true)
    , m_isValid(true)
    , m_wasChangedSinceLastFormControlChangeEvent(false)
    , m_wasFocusedByMouse(false)
    , m_hasAutofocused(false)
{
    setForm(form ? form : findFormAncestor());
    setHasCustomStyleCallbacks();
}

HTMLFormControlElement::~HTMLFormControlElement()
{
    setForm(0);
}

String HTMLFormControlElement::formEnctype() const
{
    const AtomicString& formEnctypeAttr = fastGetAttribute(formenctypeAttr);
    if (formEnctypeAttr.isNull())
        return emptyString();
    return FormSubmission::Attributes::parseEncodingType(formEnctypeAttr);
}

void HTMLFormControlElement::setFormEnctype(const String& value)
{
    setAttribute(formenctypeAttr, value);
}

String HTMLFormControlElement::formMethod() const
{
    const AtomicString& formMethodAttr = fastGetAttribute(formmethodAttr);
    if (formMethodAttr.isNull())
        return emptyString();
    return FormSubmission::Attributes::methodString(FormSubmission::Attributes::parseMethodType(formMethodAttr));
}

void HTMLFormControlElement::setFormMethod(const String& value)
{
    setAttribute(formmethodAttr, value);
}

bool HTMLFormControlElement::formNoValidate() const
{
    return fastHasAttribute(formnovalidateAttr);
}

void HTMLFormControlElement::updateAncestorDisabledState() const
{
    HTMLFieldSetElement* fieldSetAncestor = 0;
    ContainerNode* legendAncestor = 0;
    for (ContainerNode* ancestor = parentNode(); ancestor; ancestor = ancestor->parentNode()) {
        if (!legendAncestor && ancestor->hasTagName(legendTag))
            legendAncestor = ancestor;
        if (ancestor->hasTagName(fieldsetTag)) {
            fieldSetAncestor = toHTMLFieldSetElement(ancestor);
            break;
        }
    }
    m_ancestorDisabledState = (fieldSetAncestor && fieldSetAncestor->isDisabledFormControl() && !(legendAncestor && legendAncestor == fieldSetAncestor->legend())) ? AncestorDisabledStateDisabled : AncestorDisabledStateEnabled;
}

void HTMLFormControlElement::ancestorDisabledStateWasChanged()
{
    m_ancestorDisabledState = AncestorDisabledStateUnknown;
    disabledAttributeChanged();
}

void HTMLFormControlElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == formAttr) {
        formAttributeChanged();
        UseCounter::count(&document(), UseCounter::FormAttribute);
    } else if (name == disabledAttr) {
        bool oldDisabled = m_disabled;
        m_disabled = !value.isNull();
        if (oldDisabled != m_disabled)
            disabledAttributeChanged();
    } else if (name == readonlyAttr) {
        bool wasReadOnly = m_isReadOnly;
        m_isReadOnly = !value.isNull();
        if (wasReadOnly != m_isReadOnly) {
            setNeedsWillValidateCheck();
            setNeedsStyleRecalc();
            if (renderer() && renderer()->style()->hasAppearance())
                RenderTheme::theme().stateChanged(renderer(), ReadOnlyState);
        }
    } else if (name == requiredAttr) {
        bool wasRequired = m_isRequired;
        m_isRequired = !value.isNull();
        if (wasRequired != m_isRequired)
            requiredAttributeChanged();
        UseCounter::count(&document(), UseCounter::RequiredAttribute);
    } else if (name == autofocusAttr) {
        HTMLElement::parseAttribute(name, value);
        UseCounter::count(&document(), UseCounter::AutoFocusAttribute);
    } else
        HTMLElement::parseAttribute(name, value);
}

void HTMLFormControlElement::disabledAttributeChanged()
{
    setNeedsWillValidateCheck();
    didAffectSelector(AffectedSelectorDisabled | AffectedSelectorEnabled);
    if (renderer() && renderer()->style()->hasAppearance())
        RenderTheme::theme().stateChanged(renderer(), EnabledState);
    if (isDisabledFormControl() && treeScope().adjustedFocusedElement() == this) {
        // We might want to call blur(), but it's dangerous to dispatch events
        // here.
        document().setNeedsFocusedElementCheck();
    }
}

void HTMLFormControlElement::requiredAttributeChanged()
{
    setNeedsValidityCheck();
    // Style recalculation is needed because style selectors may include
    // :required and :optional pseudo-classes.
    setNeedsStyleRecalc();
}

static bool shouldAutofocus(HTMLFormControlElement* element)
{
    if (!element->fastHasAttribute(autofocusAttr))
        return false;
    if (!element->renderer())
        return false;
    if (element->document().ignoreAutofocus())
        return false;
    if (element->document().isSandboxed(SandboxAutomaticFeatures)) {
        // FIXME: This message should be moved off the console once a solution to https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
        element->document().addConsoleMessage(SecurityMessageSource, ErrorMessageLevel, "Blocked autofocusing on a form control because the form's frame is sandboxed and the 'allow-scripts' permission is not set.");
        return false;
    }
    if (element->hasAutofocused())
        return false;

    // FIXME: Should this set of hasTagName checks be replaced by a
    // virtual member function?
    if (element->hasTagName(inputTag))
        return !toHTMLInputElement(element)->isInputTypeHidden();
    if (element->hasTagName(selectTag))
        return true;
    if (element->hasTagName(keygenTag))
        return true;
    if (element->hasTagName(buttonTag))
        return true;
    if (isHTMLTextAreaElement(element))
        return true;

    return false;
}

static void focusPostAttach(Node* element)
{
    toElement(element)->focus();
    element->deref();
}

void HTMLFormControlElement::attach(const AttachContext& context)
{
    HTMLElement::attach(context);

    // The call to updateFromElement() needs to go after the call through
    // to the base class's attach() because that can sometimes do a close
    // on the renderer.
    if (renderer())
        renderer()->updateFromElement();

    if (shouldAutofocus(this)) {
        setAutofocused();
        ref();
        PostAttachCallbacks::queueCallback(focusPostAttach, this);
    }
}

void HTMLFormControlElement::didMoveToNewDocument(Document* oldDocument)
{
    FormAssociatedElement::didMoveToNewDocument(oldDocument);
    HTMLElement::didMoveToNewDocument(oldDocument);
}

Node::InsertionNotificationRequest HTMLFormControlElement::insertedInto(ContainerNode* insertionPoint)
{
    m_ancestorDisabledState = AncestorDisabledStateUnknown;
    m_dataListAncestorState = Unknown;
    setNeedsWillValidateCheck();
    HTMLElement::insertedInto(insertionPoint);
    FormAssociatedElement::insertedInto(insertionPoint);
    return InsertionDone;
}

void HTMLFormControlElement::removedFrom(ContainerNode* insertionPoint)
{
    m_validationMessage = nullptr;
    m_ancestorDisabledState = AncestorDisabledStateUnknown;
    m_dataListAncestorState = Unknown;
    HTMLElement::removedFrom(insertionPoint);
    FormAssociatedElement::removedFrom(insertionPoint);
}

bool HTMLFormControlElement::wasChangedSinceLastFormControlChangeEvent() const
{
    return m_wasChangedSinceLastFormControlChangeEvent;
}

void HTMLFormControlElement::setChangedSinceLastFormControlChangeEvent(bool changed)
{
    m_wasChangedSinceLastFormControlChangeEvent = changed;
}

void HTMLFormControlElement::dispatchFormControlChangeEvent()
{
    HTMLElement::dispatchChangeEvent();
    setChangedSinceLastFormControlChangeEvent(false);
}

void HTMLFormControlElement::dispatchFormControlInputEvent()
{
    setChangedSinceLastFormControlChangeEvent(true);
    HTMLElement::dispatchInputEvent();
}

bool HTMLFormControlElement::isDisabledFormControl() const
{
    if (m_disabled)
        return true;

    if (m_ancestorDisabledState == AncestorDisabledStateUnknown)
        updateAncestorDisabledState();
    return m_ancestorDisabledState == AncestorDisabledStateDisabled;
}

bool HTMLFormControlElement::isRequired() const
{
    return m_isRequired;
}

static void updateFromElementCallback(Node* node)
{
    ASSERT_ARG(node, node->isElementNode());
    ASSERT_ARG(node, toElement(node)->isFormControlElement());
    if (RenderObject* renderer = node->renderer())
        renderer->updateFromElement();
}

void HTMLFormControlElement::didRecalcStyle(StyleRecalcChange)
{
    // updateFromElement() can cause the selection to change, and in turn
    // trigger synchronous layout, so it must not be called during style recalc.
    if (renderer())
        PostAttachCallbacks::queueCallback(updateFromElementCallback, this);
}

bool HTMLFormControlElement::supportsFocus() const
{
    return !isDisabledFormControl();
}

bool HTMLFormControlElement::isKeyboardFocusable() const
{
    // Skip tabIndex check in a parent class.
    return isFocusable();
}

bool HTMLFormControlElement::shouldShowFocusRingOnMouseFocus() const
{
    return false;
}

void HTMLFormControlElement::dispatchFocusEvent(Element* oldFocusedElement, FocusDirection direction)
{
    if (direction != FocusDirectionPage)
        m_wasFocusedByMouse = direction == FocusDirectionMouse;
    HTMLElement::dispatchFocusEvent(oldFocusedElement, direction);
}

bool HTMLFormControlElement::shouldHaveFocusAppearance() const
{
    ASSERT(focused());
    return shouldShowFocusRingOnMouseFocus() || !m_wasFocusedByMouse;
}

void HTMLFormControlElement::willCallDefaultEventHandler(const Event& event)
{
    if (!event.isKeyboardEvent() || event.type() != eventNames().keydownEvent)
        return;
    if (!m_wasFocusedByMouse)
        return;
    m_wasFocusedByMouse = false;
    if (renderer())
        renderer()->repaint();
}


short HTMLFormControlElement::tabIndex() const
{
    // Skip the supportsFocus check in HTMLElement.
    return Element::tabIndex();
}

bool HTMLFormControlElement::recalcWillValidate() const
{
    if (m_dataListAncestorState == Unknown) {
        for (ContainerNode* ancestor = parentNode(); ancestor; ancestor = ancestor->parentNode()) {
            if (ancestor->hasTagName(datalistTag)) {
                m_dataListAncestorState = InsideDataList;
                break;
            }
        }
        if (m_dataListAncestorState == Unknown)
            m_dataListAncestorState = NotInsideDataList;
    }
    return m_dataListAncestorState == NotInsideDataList && !isDisabledOrReadOnly();
}

bool HTMLFormControlElement::willValidate() const
{
    if (!m_willValidateInitialized || m_dataListAncestorState == Unknown) {
        m_willValidateInitialized = true;
        bool newWillValidate = recalcWillValidate();
        if (m_willValidate != newWillValidate) {
            m_willValidate = newWillValidate;
            const_cast<HTMLFormControlElement*>(this)->setNeedsValidityCheck();
        }
    } else {
        // If the following assertion fails, setNeedsWillValidateCheck() is not
        // called correctly when something which changes recalcWillValidate() result
        // is updated.
        ASSERT(m_willValidate == recalcWillValidate());
    }
    return m_willValidate;
}

void HTMLFormControlElement::setNeedsWillValidateCheck()
{
    // We need to recalculate willValidate immediately because willValidate change can causes style change.
    bool newWillValidate = recalcWillValidate();
    if (m_willValidateInitialized && m_willValidate == newWillValidate)
        return;
    m_willValidateInitialized = true;
    m_willValidate = newWillValidate;
    setNeedsValidityCheck();
    setNeedsStyleRecalc();
    if (!m_willValidate)
        hideVisibleValidationMessage();
}

void HTMLFormControlElement::updateVisibleValidationMessage()
{
    Page* page = document().page();
    if (!page)
        return;
    String message;
    if (renderer() && willValidate())
        message = validationMessage().stripWhiteSpace();
    if (!m_validationMessage)
        m_validationMessage = ValidationMessage::create(this);
    m_validationMessage->updateValidationMessage(message);
}

void HTMLFormControlElement::hideVisibleValidationMessage()
{
    if (m_validationMessage)
        m_validationMessage->requestToHideMessage();
}

bool HTMLFormControlElement::checkValidity(Vector<RefPtr<FormAssociatedElement> >* unhandledInvalidControls, CheckValidityDispatchEvents dispatchEvents)
{
    if (!willValidate() || isValidFormControlElement())
        return true;
    if (dispatchEvents == CheckValidityDispatchEventsNone)
        return false;
    // An event handler can deref this object.
    RefPtr<HTMLFormControlElement> protector(this);
    RefPtr<Document> originalDocument(&document());
    bool needsDefaultAction = dispatchEvent(Event::createCancelable(eventNames().invalidEvent));
    if (needsDefaultAction && unhandledInvalidControls && inDocument() && originalDocument == &document())
        unhandledInvalidControls->append(this);
    return false;
}

bool HTMLFormControlElement::isValidFormControlElement()
{
    // If the following assertion fails, setNeedsValidityCheck() is not called
    // correctly when something which changes validity is updated.
    ASSERT(m_isValid == validity()->valid());
    return m_isValid;
}

void HTMLFormControlElement::setNeedsValidityCheck()
{
    bool newIsValid = validity()->valid();
    if (willValidate() && newIsValid != m_isValid) {
        // Update style for pseudo classes such as :valid :invalid.
        setNeedsStyleRecalc();
    }
    m_isValid = newIsValid;

    // Updates only if this control already has a validtion message.
    if (m_validationMessage && m_validationMessage->isVisible()) {
        // Calls updateVisibleValidationMessage() even if m_isValid is not
        // changed because a validation message can be chagned.
        updateVisibleValidationMessage();
    }
}

void HTMLFormControlElement::setCustomValidity(const String& error)
{
    FormAssociatedElement::setCustomValidity(error);
    setNeedsValidityCheck();
}

void HTMLFormControlElement::dispatchBlurEvent(Element* newFocusedElement)
{
    HTMLElement::dispatchBlurEvent(newFocusedElement);
    hideVisibleValidationMessage();
}

HTMLFormElement* HTMLFormControlElement::virtualForm() const
{
    return FormAssociatedElement::form();
}

bool HTMLFormControlElement::isDefaultButtonForForm() const
{
    return isSuccessfulSubmitButton() && form() && form()->defaultButton() == this;
}

HTMLFormControlElement* HTMLFormControlElement::enclosingFormControlElement(Node* node)
{
    for (; node; node = node->parentNode()) {
        if (node->isElementNode() && toElement(node)->isFormControlElement())
            return toHTMLFormControlElement(node);
    }
    return 0;
}

} // namespace Webcore
