/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2004, 2005, 2006, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Motorola Mobility, Inc.  All rights reserved.
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
#include "core/html/HTMLOptionElement.h"

#include "HTMLNames.h"
#include "bindings/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/NodeRenderStyle.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/ScriptLoader.h"
#include "core/dom/Text.h"
#include "core/html/HTMLDataListElement.h"
#include "core/html/HTMLOptGroupElement.h"
#include "core/html/HTMLSelectElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/rendering/RenderTheme.h"
#include "wtf/Vector.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

using namespace HTMLNames;

HTMLOptionElement::HTMLOptionElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
    , m_disabled(false)
    , m_isSelected(false)
{
    ASSERT(hasTagName(optionTag));
    setHasCustomStyleCallbacks();
    ScriptWrappable::init(this);
}

PassRefPtr<HTMLOptionElement> HTMLOptionElement::create(Document& document)
{
    return adoptRef(new HTMLOptionElement(optionTag, document));
}

PassRefPtr<HTMLOptionElement> HTMLOptionElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(new HTMLOptionElement(tagName, document));
}

PassRefPtr<HTMLOptionElement> HTMLOptionElement::createForJSConstructor(Document& document, const String& data, const String& value,
    bool defaultSelected, bool selected, ExceptionState& es)
{
    RefPtr<HTMLOptionElement> element = adoptRef(new HTMLOptionElement(optionTag, document));

    RefPtr<Text> text = Text::create(document, data.isNull() ? "" : data);

    element->appendChild(text.release(), es);
    if (es.hadException())
        return 0;

    if (!value.isNull())
        element->setValue(value);
    if (defaultSelected)
        element->setAttribute(selectedAttr, emptyAtom);
    element->setSelected(selected);

    return element.release();
}

void HTMLOptionElement::attach(const AttachContext& context)
{
    HTMLElement::attach(context);
    // If after attaching nothing called styleForRenderer() on this node we
    // manually cache the value. This happens if our parent doesn't have a
    // renderer like <optgroup> or if it doesn't allow children like <select>.
    if (!m_style && parentNode()->renderStyle())
        updateNonRenderStyle();
}

void HTMLOptionElement::detach(const AttachContext& context)
{
    m_style.clear();
    HTMLElement::detach(context);
}

bool HTMLOptionElement::rendererIsFocusable() const
{
    // Option elements do not have a renderer so we check the renderStyle instead.
    return renderStyle() && renderStyle()->display() != NONE;
}

String HTMLOptionElement::text() const
{
    Document& document = this->document();
    String text;

    // WinIE does not use the label attribute, so as a quirk, we ignore it.
    if (!document.inQuirksMode())
        text = fastGetAttribute(labelAttr);

    // FIXME: The following treats an element with the label attribute set to
    // the empty string the same as an element with no label attribute at all.
    // Is that correct? If it is, then should the label function work the same way?
    if (text.isEmpty())
        text = collectOptionInnerText();

    // FIXME: Is displayStringModifiedByEncoding helpful here?
    // If it's correct here, then isn't it needed in the value and label functions too?
    return document.displayStringModifiedByEncoding(text).stripWhiteSpace(isHTMLSpace).simplifyWhiteSpace(isHTMLSpace);
}

void HTMLOptionElement::setText(const String &text, ExceptionState& es)
{
    RefPtr<Node> protectFromMutationEvents(this);

    // Changing the text causes a recalc of a select's items, which will reset the selected
    // index to the first item if the select is single selection with a menu list. We attempt to
    // preserve the selected item.
    RefPtr<HTMLSelectElement> select = ownerSelectElement();
    bool selectIsMenuList = select && select->usesMenuList();
    int oldSelectedIndex = selectIsMenuList ? select->selectedIndex() : -1;

    // Handle the common special case where there's exactly 1 child node, and it's a text node.
    Node* child = firstChild();
    if (child && child->isTextNode() && !child->nextSibling())
        toText(child)->setData(text);
    else {
        removeChildren();
        appendChild(Text::create(document(), text), es);
    }

    if (selectIsMenuList && select->selectedIndex() != oldSelectedIndex)
        select->setSelectedIndex(oldSelectedIndex);
}

void HTMLOptionElement::accessKeyAction(bool)
{
    HTMLSelectElement* select = ownerSelectElement();
    if (select)
        select->accessKeySetSelectedIndex(index());
}

int HTMLOptionElement::index() const
{
    // It would be faster to cache the index, but harder to get it right in all cases.

    HTMLSelectElement* selectElement = ownerSelectElement();
    if (!selectElement)
        return 0;

    int optionIndex = 0;

    const Vector<HTMLElement*>& items = selectElement->listItems();
    size_t length = items.size();
    for (size_t i = 0; i < length; ++i) {
        if (!items[i]->hasTagName(optionTag))
            continue;
        if (items[i] == this)
            return optionIndex;
        ++optionIndex;
    }

    return 0;
}

void HTMLOptionElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == valueAttr) {
        if (HTMLDataListElement* dataList = ownerDataListElement())
            dataList->optionElementChildrenChanged();
    } else if (name == disabledAttr) {
        bool oldDisabled = m_disabled;
        m_disabled = !value.isNull();
        if (oldDisabled != m_disabled) {
            didAffectSelector(AffectedSelectorDisabled | AffectedSelectorEnabled);
            if (renderer() && renderer()->style()->hasAppearance())
                RenderTheme::theme().stateChanged(renderer(), EnabledState);
        }
    } else if (name == selectedAttr) {
        if (bool willBeSelected = !value.isNull())
            setSelected(willBeSelected);
    } else
        HTMLElement::parseAttribute(name, value);
}

String HTMLOptionElement::value() const
{
    const AtomicString& value = fastGetAttribute(valueAttr);
    if (!value.isNull())
        return value;
    return collectOptionInnerText().stripWhiteSpace(isHTMLSpace).simplifyWhiteSpace(isHTMLSpace);
}

void HTMLOptionElement::setValue(const String& value)
{
    setAttribute(valueAttr, value);
}

bool HTMLOptionElement::selected()
{
    if (HTMLSelectElement* select = ownerSelectElement()) {
        // If a stylesheet contains option:checked selectors, this function is
        // called during parsing. updateListItemSelectedStates() is O(N) where N
        // is the number of option elements, so the <select> parsing would be
        // O(N^2) without isParsingInProgress check. Also,
        // updateListItemSelectedStates() determines default selection, and we'd
        // like to avoid to determine default selection with incomplete option
        // list.
        if (select->isParsingInProgress())
            return m_isSelected;
        select->updateListItemSelectedStates();
    }
    return m_isSelected;
}

void HTMLOptionElement::setSelected(bool selected)
{
    if (m_isSelected == selected)
        return;

    setSelectedState(selected);

    if (HTMLSelectElement* select = ownerSelectElement())
        select->optionSelectionStateChanged(this, selected);
}

void HTMLOptionElement::setSelectedState(bool selected)
{
    if (m_isSelected == selected)
        return;

    m_isSelected = selected;
    didAffectSelector(AffectedSelectorChecked);

    if (HTMLSelectElement* select = ownerSelectElement())
        select->invalidateSelectedItems();
}

void HTMLOptionElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    if (HTMLDataListElement* dataList = ownerDataListElement())
        dataList->optionElementChildrenChanged();
    else if (HTMLSelectElement* select = ownerSelectElement())
        select->optionElementChildrenChanged();
    HTMLElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
}

HTMLDataListElement* HTMLOptionElement::ownerDataListElement() const
{
    for (ContainerNode* parent = parentNode(); parent ; parent = parent->parentNode()) {
        if (parent->hasTagName(datalistTag))
            return toHTMLDataListElement(parent);
    }
    return 0;
}

HTMLSelectElement* HTMLOptionElement::ownerSelectElement() const
{
    ContainerNode* select = parentNode();
    while (select && !select->hasTagName(selectTag))
        select = select->parentNode();

    if (!select)
        return 0;

    return toHTMLSelectElement(select);
}

String HTMLOptionElement::label() const
{
    const AtomicString& label = fastGetAttribute(labelAttr);
    if (!label.isNull())
        return label;
    return collectOptionInnerText().stripWhiteSpace(isHTMLSpace).simplifyWhiteSpace(isHTMLSpace);
}

void HTMLOptionElement::setLabel(const String& label)
{
    setAttribute(labelAttr, label);
}

void HTMLOptionElement::updateNonRenderStyle()
{
    m_style = originalStyleForRenderer();
}

RenderStyle* HTMLOptionElement::nonRendererStyle() const
{
    return m_style.get();
}

PassRefPtr<RenderStyle> HTMLOptionElement::customStyleForRenderer()
{
    // styleForRenderer is called whenever a new style should be associated
    // with an Element so now is a good time to update our cached style.
    updateNonRenderStyle();
    return m_style;
}

void HTMLOptionElement::didRecalcStyle(StyleRecalcChange)
{
    // FIXME: This is nasty, we ask our owner select to repaint even if the new
    // style is exactly the same.
    if (HTMLSelectElement* select = ownerSelectElement()) {
        if (RenderObject* renderer = select->renderer())
            renderer->repaint();
    }
}

String HTMLOptionElement::textIndentedToRespectGroupLabel() const
{
    ContainerNode* parent = parentNode();
    if (parent && isHTMLOptGroupElement(parent))
        return "    " + text();
    return text();
}

bool HTMLOptionElement::isDisabledFormControl() const
{
    if (ownElementDisabled())
        return true;
    if (Element* parent = parentElement())
        return isHTMLOptGroupElement(parent) && parent->isDisabledFormControl();
    return false;
}

Node::InsertionNotificationRequest HTMLOptionElement::insertedInto(ContainerNode* insertionPoint)
{
    if (HTMLSelectElement* select = ownerSelectElement()) {
        select->setRecalcListItems();
        // Do not call selected() since calling updateListItemSelectedStates()
        // at this time won't do the right thing. (Why, exactly?)
        // FIXME: Might be better to call this unconditionally, always passing m_isSelected,
        // rather than only calling it if we are selected.
        if (m_isSelected)
            select->optionSelectionStateChanged(this, true);
        select->scrollToSelection();
    }

    return HTMLElement::insertedInto(insertionPoint);
}

String HTMLOptionElement::collectOptionInnerText() const
{
    StringBuilder text;
    for (Node* node = firstChild(); node; ) {
        if (node->isTextNode())
            text.append(node->nodeValue());
        // Text nodes inside script elements are not part of the option text.
        if (node->isElementNode() && toScriptLoaderIfPossible(toElement(node)))
            node = NodeTraversal::nextSkippingChildren(node, this);
        else
            node = NodeTraversal::next(node, this);
    }
    return text.toString();
}

} // namespace WebCore
