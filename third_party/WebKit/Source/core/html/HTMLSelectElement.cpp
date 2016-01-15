/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009, 2010, 2011 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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

#include "core/html/HTMLSelectElement.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "bindings/core/v8/UnionTypesCore.h"
#include "core/HTMLNames.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/Attribute.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/NodeListsNodeData.h"
#include "core/dom/NodeTraversal.h"
#include "core/events/GestureEvent.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/MouseEvent.h"
#include "core/events/ScopedEventQueue.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/html/FormData.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLOptGroupElement.h"
#include "core/html/HTMLOptionElement.h"
#include "core/html/forms/FormController.h"
#include "core/input/EventHandler.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/layout/HitTestRequest.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutListBox.h"
#include "core/layout/LayoutMenuList.h"
#include "core/layout/LayoutText.h"
#include "core/layout/LayoutTheme.h"
#include "core/layout/LayoutView.h"
#include "core/page/AutoscrollController.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/page/SpatialNavigation.h"
#include "platform/PlatformMouseEvent.h"
#include "platform/PopupMenu.h"
#include "platform/TraceEvent.h"
#include "platform/text/PlatformLocale.h"

using namespace WTF::Unicode;

namespace blink {

using namespace HTMLNames;

// Upper limit of m_listItems. According to the HTML standard, options larger
// than this limit doesn't work well because |selectedIndex| IDL attribute is
// signed.
static const unsigned maxListItems = INT_MAX;

HTMLSelectElement::HTMLSelectElement(Document& document, HTMLFormElement* form)
    : HTMLFormControlElementWithState(selectTag, document, form)
    , m_typeAhead(this)
    , m_size(0)
    , m_lastOnChangeOption(nullptr)
    , m_activeSelectionAnchorIndex(-1)
    , m_activeSelectionEndIndex(-1)
    , m_multiple(false)
    , m_activeSelectionState(false)
    , m_shouldRecalcListItems(false)
    , m_suggestedIndex(-1)
    , m_isAutofilledByPreview(false)
    , m_indexToSelectOnCancel(-1)
    , m_popupIsVisible(false)
{
    setHasCustomStyleCallbacks();
}

PassRefPtrWillBeRawPtr<HTMLSelectElement> HTMLSelectElement::create(Document& document)
{
    RefPtrWillBeRawPtr<HTMLSelectElement> select = adoptRefWillBeNoop(new HTMLSelectElement(document, 0));
    select->ensureUserAgentShadowRoot();
    return select.release();
}

PassRefPtrWillBeRawPtr<HTMLSelectElement> HTMLSelectElement::create(Document& document, HTMLFormElement* form)
{
    RefPtrWillBeRawPtr<HTMLSelectElement> select = adoptRefWillBeNoop(new HTMLSelectElement(document, form));
    select->ensureUserAgentShadowRoot();
    return select.release();
}

HTMLSelectElement::~HTMLSelectElement()
{
}

const AtomicString& HTMLSelectElement::formControlType() const
{
    DEFINE_STATIC_LOCAL(const AtomicString, selectMultiple, ("select-multiple", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(const AtomicString, selectOne, ("select-one", AtomicString::ConstructFromLiteral));
    return m_multiple ? selectMultiple : selectOne;
}

void HTMLSelectElement::optionSelectedByUser(int optionIndex, bool fireOnChangeNow, bool allowMultipleSelection)
{
    // User interaction such as mousedown events can cause list box select
    // elements to send change events.  This produces that same behavior for
    // changes triggered by other code running on behalf of the user.
    if (!usesMenuList()) {
        updateSelectedState(optionToListIndex(optionIndex), allowMultipleSelection, false);
        setNeedsValidityCheck();
        if (fireOnChangeNow)
            listBoxOnChange();
        return;
    }

    // Bail out if this index is already the selected one, to avoid running
    // unnecessary JavaScript that can mess up autofill when there is no actual
    // change (see https://bugs.webkit.org/show_bug.cgi?id=35256 and
    // <rdar://7467917>).  The selectOption function does not behave this way,
    // possibly because other callers need a change event even in cases where
    // the selected option is not change.
    if (optionIndex == selectedIndex())
        return;

    selectOption(optionIndex, DeselectOtherOptions | MakeOptionDirty | (fireOnChangeNow ? DispatchInputAndChangeEvent : 0));
}

bool HTMLSelectElement::hasPlaceholderLabelOption() const
{
    // The select element has no placeholder label option if it has an attribute
    // "multiple" specified or a display size of non-1.
    //
    // The condition "size() > 1" is not compliant with the HTML5 spec as of Dec
    // 3, 2010. "size() != 1" is correct.  Using "size() > 1" here because
    // size() may be 0 in WebKit.  See the discussion at
    // https://bugs.webkit.org/show_bug.cgi?id=43887
    //
    // "0 size()" happens when an attribute "size" is absent or an invalid size
    // attribute is specified.  In this case, the display size should be assumed
    // as the default.  The default display size is 1 for non-multiple select
    // elements, and 4 for multiple select elements.
    //
    // Finally, if size() == 0 and non-multiple, the display size can be assumed
    // as 1.
    if (multiple() || size() > 1)
        return false;

    int listIndex = optionToListIndex(0);
    ASSERT(listIndex >= 0);
    if (listIndex < 0)
        return false;
    return !listIndex && toHTMLOptionElement(listItems()[listIndex])->value().isEmpty();
}

String HTMLSelectElement::validationMessage() const
{
    if (!willValidate())
        return String();
    if (customError())
        return customValidationMessage();
    if (valueMissing())
        return locale().queryString(WebLocalizedString::ValidationValueMissingForSelect);
    return String();
}

bool HTMLSelectElement::valueMissing() const
{
    if (!willValidate())
        return false;

    if (!isRequired())
        return false;

    int firstSelectionIndex = selectedIndex();

    // If a non-placeholer label option is selected (firstSelectionIndex > 0),
    // it's not value-missing.
    return firstSelectionIndex < 0 || (!firstSelectionIndex && hasPlaceholderLabelOption());
}

void HTMLSelectElement::listBoxSelectItem(int listIndex, bool allowMultiplySelections, bool shift, bool fireOnChangeNow)
{
    if (!multiple()) {
        optionSelectedByUser(listToOptionIndex(listIndex), fireOnChangeNow, false);
    } else {
        updateSelectedState(listIndex, allowMultiplySelections, shift);
        setNeedsValidityCheck();
        if (fireOnChangeNow)
            listBoxOnChange();
    }
}

bool HTMLSelectElement::usesMenuList() const
{
    if (LayoutTheme::theme().delegatesMenuListRendering())
        return true;

    return !m_multiple && m_size <= 1;
}

int HTMLSelectElement::activeSelectionEndListIndex() const
{
    if (m_activeSelectionEndIndex >= 0)
        return m_activeSelectionEndIndex;
    return lastSelectedListIndex();
}

void HTMLSelectElement::add(const HTMLOptionElementOrHTMLOptGroupElement& element, const HTMLElementOrLong& before, ExceptionState& exceptionState)
{
    RefPtrWillBeRawPtr<HTMLElement> elementToInsert;
    ASSERT(!element.isNull());
    if (element.isHTMLOptionElement())
        elementToInsert = element.getAsHTMLOptionElement();
    else
        elementToInsert = element.getAsHTMLOptGroupElement();

    RefPtrWillBeRawPtr<HTMLElement> beforeElement;
    if (before.isHTMLElement())
        beforeElement = before.getAsHTMLElement();
    else if (before.isLong())
        beforeElement = options()->item(before.getAsLong());
    else
        beforeElement = nullptr;

    insertBefore(elementToInsert, beforeElement.get(), exceptionState);
    setNeedsValidityCheck();
}

void HTMLSelectElement::remove(int optionIndex)
{
    int listIndex = optionToListIndex(optionIndex);
    if (listIndex >= 0)
        listItems()[listIndex]->remove(IGNORE_EXCEPTION);
}

String HTMLSelectElement::value() const
{
    if (HTMLOptionElement* option = selectedOption())
        return option->value();
    return "";
}

void HTMLSelectElement::setValue(const String &value, bool sendEvents)
{
    // We clear the previously selected option(s) when needed, to guarantee
    // calling setSelectedIndex() only once.
    int optionIndex = 0;
    if (value.isNull()) {
        optionIndex = -1;
    } else {
        // Find the option with value() matching the given parameter and make it
        // the current selection.
        for (auto& item : listItems()) {
            if (!isHTMLOptionElement(item))
                continue;
            if (toHTMLOptionElement(item)->value() == value)
                break;
            optionIndex++;
        }
        if (optionIndex >= static_cast<int>(listItems().size()))
            optionIndex = -1;
    }

    int previousSelectedIndex = selectedIndex();
    setSuggestedIndex(-1);
    if (m_isAutofilledByPreview)
        setAutofilled(false);
    SelectOptionFlags flags = DeselectOtherOptions | MakeOptionDirty;
    if (sendEvents)
        flags |= DispatchInputAndChangeEvent;
    selectOption(optionIndex, flags);

    if (sendEvents && previousSelectedIndex != selectedIndex() && !usesMenuList())
        listBoxOnChange();
}

String HTMLSelectElement::suggestedValue() const
{
    const ListItems& items = listItems();
    for (unsigned i = 0; i < items.size(); ++i) {
        if (isHTMLOptionElement(items[i]) && m_suggestedIndex >= 0) {
            if (i == static_cast<unsigned>(m_suggestedIndex))
                return toHTMLOptionElement(items[i])->value();
        }
    }
    return "";
}

void HTMLSelectElement::setSuggestedValue(const String& value)
{
    if (value.isNull()) {
        setSuggestedIndex(-1);
        return;
    }

    unsigned optionIndex = 0;
    for (auto& item : listItems()) {
        if (!isHTMLOptionElement(item))
            continue;
        if (toHTMLOptionElement(item)->value() == value) {
            setSuggestedIndex(optionIndex);
            m_isAutofilledByPreview = true;
            return;
        }
        optionIndex++;
    }

    setSuggestedIndex(-1);
}

bool HTMLSelectElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == alignAttr) {
        // Don't map 'align' attribute. This matches what Firefox, Opera and IE do.
        // See http://bugs.webkit.org/show_bug.cgi?id=12072
        return false;
    }

    return HTMLFormControlElementWithState::isPresentationAttribute(name);
}

void HTMLSelectElement::parseAttribute(const QualifiedName& name, const AtomicString& oldValue, const AtomicString& value)
{
    if (name == sizeAttr) {
        unsigned oldSize = m_size;
        // Set the attribute value to a number.
        // This is important since the style rules for this attribute can
        // determine the appearance property.
        unsigned size = value.string().toUInt();
        AtomicString attrSize = AtomicString::number(size);
        if (attrSize != value) {
            // FIXME: This is horribly factored.
            if (Attribute* sizeAttribute = ensureUniqueElementData().attributes().find(sizeAttr))
                sizeAttribute->setValue(attrSize);
        }
        size = std::max(size, 0u);

        // Ensure that we've determined selectedness of the items at least once
        // prior to changing the size.
        if (oldSize != size)
            updateListItemSelectedStates();

        m_size = size;
        setNeedsValidityCheck();
        if (m_size != oldSize && inActiveDocument()) {
            lazyReattachIfAttached();
            setRecalcListItems();
        }
    } else if (name == multipleAttr) {
        parseMultipleAttribute(value);
    } else if (name == accesskeyAttr) {
        // FIXME: ignore for the moment.
        //
    } else if (name == disabledAttr) {
        HTMLFormControlElementWithState::parseAttribute(name, oldValue, value);
        if (popupIsVisible())
            hidePopup();

    } else {
        HTMLFormControlElementWithState::parseAttribute(name, oldValue, value);
    }
}

bool HTMLSelectElement::shouldShowFocusRingOnMouseFocus() const
{
    return true;
}

bool HTMLSelectElement::canSelectAll() const
{
    return !usesMenuList();
}

LayoutObject* HTMLSelectElement::createLayoutObject(const ComputedStyle&)
{
    if (usesMenuList())
        return new LayoutMenuList(this);
    return new LayoutListBox(this);
}

PassRefPtrWillBeRawPtr<HTMLCollection> HTMLSelectElement::selectedOptions()
{
    updateListItemSelectedStates();
    return ensureCachedCollection<HTMLCollection>(SelectedOptions);
}

PassRefPtrWillBeRawPtr<HTMLOptionsCollection> HTMLSelectElement::options()
{
    return ensureCachedCollection<HTMLOptionsCollection>(SelectOptions);
}

void HTMLSelectElement::updateListItemSelectedStates()
{
    if (!m_shouldRecalcListItems)
        return;
    recalcListItems();
    setNeedsValidityCheck();
}

void HTMLSelectElement::childrenChanged(const ChildrenChange& change)
{
    setRecalcListItems();
    setNeedsValidityCheck();
    m_lastOnChangeSelection.clear();

    HTMLFormControlElementWithState::childrenChanged(change);
}

void HTMLSelectElement::optionElementChildrenChanged()
{
    setNeedsValidityCheck();

    if (layoutObject()) {
        if (AXObjectCache* cache = layoutObject()->document().existingAXObjectCache())
            cache->childrenChanged(this);
    }
}

void HTMLSelectElement::accessKeyAction(bool sendMouseEvents)
{
    focus();
    dispatchSimulatedClick(nullptr, sendMouseEvents ? SendMouseUpDownEvents : SendNoEvents);
}

void HTMLSelectElement::setMultiple(bool multiple)
{
    bool oldMultiple = this->multiple();
    int oldSelectedIndex = selectedIndex();
    setAttribute(multipleAttr, multiple ? emptyAtom : nullAtom);

    // Restore selectedIndex after changing the multiple flag to preserve
    // selection as single-line and multi-line has different defaults.
    if (oldMultiple != this->multiple())
        selectOption(oldSelectedIndex, DeselectOtherOptions);
}

void HTMLSelectElement::setSize(unsigned size)
{
    setUnsignedIntegralAttribute(sizeAttr, size);
}

Element* HTMLSelectElement::namedItem(const AtomicString& name)
{
    return options()->namedItem(name);
}

HTMLOptionElement* HTMLSelectElement::item(unsigned index)
{
    return options()->item(index);
}

void HTMLSelectElement::setOption(unsigned index, HTMLOptionElement* option, ExceptionState& exceptionState)
{
    int diff = index - length();
    // We should check |index >= maxListItems| first to avoid integer overflow.
    if (index >= maxListItems || listItems().size() + diff + 1 > maxListItems) {
        document().addConsoleMessage(ConsoleMessage::create(JSMessageSource, WarningMessageLevel,
            String::format("Blocked to expand the option list and set an option at index=%u.  The maximum list length is %u.", index, maxListItems)));
        return;
    }
    HTMLOptionElementOrHTMLOptGroupElement element;
    element.setHTMLOptionElement(option);
    HTMLElementOrLong before;
    // Out of array bounds? First insert empty dummies.
    if (diff > 0) {
        setLength(index, exceptionState);
        // Replace an existing entry?
    } else if (diff < 0) {
        before.setHTMLElement(options()->item(index + 1));
        remove(index);
    }
    // Finally add the new element.
    if (!exceptionState.hadException()) {
        add(element, before, exceptionState);
        if (diff >= 0 && option->selected())
            optionSelectionStateChanged(option, true);
    }
}

void HTMLSelectElement::setLength(unsigned newLen, ExceptionState& exceptionState)
{
    // We should check |newLen > maxListItems| first to avoid integer overflow.
    if (newLen > maxListItems || listItems().size() + newLen - length() > maxListItems) {
        document().addConsoleMessage(ConsoleMessage::create(JSMessageSource, WarningMessageLevel,
            String::format("Blocked to expand the option list to %u items.  The maximum list length is %u.", newLen, maxListItems)));
        return;
    }
    int diff = length() - newLen;

    if (diff < 0) { // Add dummy elements.
        do {
            appendChild(document().createElement(optionTag, false), exceptionState);
            if (exceptionState.hadException())
                break;
        } while (++diff);
    } else {
        // Removing children fires mutation events, which might mutate the DOM
        // further, so we first copy out a list of elements that we intend to
        // remove then attempt to remove them one at a time.
        WillBeHeapVector<RefPtrWillBeMember<Element>> itemsToRemove;
        size_t optionIndex = 0;
        for (auto& item : listItems()) {
            if (isHTMLOptionElement(item) && optionIndex++ >= newLen) {
                ASSERT(item->parentNode());
                itemsToRemove.append(item.get());
            }
        }

        for (auto& item : itemsToRemove) {
            if (item->parentNode())
                item->parentNode()->removeChild(item.get(), exceptionState);
        }
    }
    setNeedsValidityCheck();
}

bool HTMLSelectElement::isRequiredFormControl() const
{
    return isRequired();
}

// Returns the 1st valid item |skip| items from |listIndex| in direction
// |direction| if there is one.
// Otherwise, it returns the valid item closest to that boundary which is past
// |listIndex| if there is one.
// Otherwise, it returns |listIndex|.
// Valid means that it is enabled and an option element.
int HTMLSelectElement::nextValidIndex(int listIndex, SkipDirection direction, int skip) const
{
    ASSERT(direction == SkipBackwards || direction == SkipForwards);
    const ListItems& listItems = this->listItems();
    int lastGoodIndex = listIndex;
    int size = listItems.size();
    for (listIndex += direction; listIndex >= 0 && listIndex < size; listIndex += direction) {
        --skip;
        HTMLElement* element = listItems[listIndex];
        if (!isHTMLOptionElement(*element))
            continue;
        if (toHTMLOptionElement(*element).isDisplayNone())
            continue;
        if (element->isDisabledFormControl())
            continue;
        if (!usesMenuList() && !element->layoutObject())
            continue;
        lastGoodIndex = listIndex;
        if (skip <= 0)
            break;
    }
    return lastGoodIndex;
}

int HTMLSelectElement::nextSelectableListIndex(int startIndex) const
{
    return nextValidIndex(startIndex, SkipForwards, 1);
}

int HTMLSelectElement::previousSelectableListIndex(int startIndex) const
{
    if (startIndex == -1)
        startIndex = listItems().size();
    return nextValidIndex(startIndex, SkipBackwards, 1);
}

int HTMLSelectElement::firstSelectableListIndex() const
{
    const ListItems& items = listItems();
    int index = nextValidIndex(items.size(), SkipBackwards, INT_MAX);
    if (static_cast<size_t>(index) == items.size())
        return -1;
    return index;
}

int HTMLSelectElement::lastSelectableListIndex() const
{
    return nextValidIndex(-1, SkipForwards, INT_MAX);
}

// Returns the index of the next valid item one page away from |startIndex| in
// direction |direction|.
int HTMLSelectElement::nextSelectableListIndexPageAway(int startIndex, SkipDirection direction) const
{
    const ListItems& items = listItems();
    // Can't use m_size because layoutObject forces a minimum size.
    int pageSize = 0;
    if (layoutObject()->isListBox())
        pageSize = toLayoutListBox(layoutObject())->size() - 1; // -1 so we still show context.

    // One page away, but not outside valid bounds.
    // If there is a valid option item one page away, the index is chosen.
    // If there is no exact one page away valid option, returns startIndex or
    // the most far index.
    int edgeIndex = (direction == SkipForwards) ? 0 : (items.size() - 1);
    int skipAmount = pageSize + ((direction == SkipForwards) ? startIndex : (edgeIndex - startIndex));
    return nextValidIndex(edgeIndex, direction, skipAmount);
}

void HTMLSelectElement::selectAll()
{
    ASSERT(!usesMenuList());
    if (!layoutObject() || !m_multiple)
        return;

    // Save the selection so it can be compared to the new selectAll selection
    // when dispatching change events.
    saveLastSelection();

    m_activeSelectionState = true;
    setActiveSelectionAnchorIndex(nextSelectableListIndex(-1));
    setActiveSelectionEndIndex(previousSelectableListIndex(-1));

    updateListBoxSelection(false, false);
    listBoxOnChange();
    setNeedsValidityCheck();
}

void HTMLSelectElement::saveLastSelection()
{
    if (usesMenuList()) {
        m_lastOnChangeOption = selectedOption();
        return;
    }

    m_lastOnChangeSelection.clear();
    for (auto& element : listItems())
        m_lastOnChangeSelection.append(isHTMLOptionElement(*element) && toHTMLOptionElement(element)->selected());
}

void HTMLSelectElement::setActiveSelectionAnchorIndex(int index)
{
    m_activeSelectionAnchorIndex = index;

    // Cache the selection state so we can restore the old selection as the new
    // selection pivots around this anchor index.
    // Example:
    // 1. Press the mouse button on the second OPTION
    //   m_activeSelectionAnchorIndex = 1
    // 2. Drag the mouse pointer onto the fifth OPTION
    //   m_activeSelectionEndIndex = 4, options at 1-4 indices are selected.
    // 3. Drag the mouse pointer onto the fourth OPTION
    //   m_activeSelectionEndIndex = 3, options at 1-3 indices are selected.
    //   updateListBoxSelection needs to clear selection of the fifth OPTION.
    m_cachedStateForActiveSelection.clear();
    for (auto& element : listItems()) {
        m_cachedStateForActiveSelection.append(isHTMLOptionElement(*element) && toHTMLOptionElement(element)->selected());
    }
}

void HTMLSelectElement::setActiveSelectionEndIndex(int index)
{
    m_activeSelectionEndIndex = index;
}

void HTMLSelectElement::updateListBoxSelection(bool deselectOtherOptions, bool scroll)
{
    ASSERT(layoutObject());
    ASSERT(layoutObject()->isListBox() || m_multiple);

    int start = std::min(m_activeSelectionAnchorIndex, m_activeSelectionEndIndex);
    int end = std::max(m_activeSelectionAnchorIndex, m_activeSelectionEndIndex);

    const ListItems& items = listItems();
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        if (!isHTMLOptionElement(*items[i]))
            continue;
        HTMLOptionElement& option = toHTMLOptionElement(*items[i]);
        if (option.isDisabledFormControl() || !option.layoutObject())
            continue;
        if (i >= start && i <= end) {
            option.setSelectedState(m_activeSelectionState);
            option.setDirty(true);
        } else if (deselectOtherOptions || i >= static_cast<int>(m_cachedStateForActiveSelection.size())) {
            option.setSelectedState(false);
            option.setDirty(true);
        } else {
            option.setSelectedState(m_cachedStateForActiveSelection[i]);
        }
    }

    setNeedsValidityCheck();
    if (scroll)
        scrollToSelection();
    notifyFormStateChanged();
}

void HTMLSelectElement::listBoxOnChange()
{
    ASSERT(!usesMenuList() || m_multiple);

    const ListItems& items = listItems();

    // If the cached selection list is empty, or the size has changed, then fire
    // dispatchFormControlChangeEvent, and return early.
    // FIXME: Why? This looks unreasonable.
    if (m_lastOnChangeSelection.isEmpty() || m_lastOnChangeSelection.size() != items.size()) {
        dispatchFormControlChangeEvent();
        return;
    }

    // Update m_lastOnChangeSelection and fire dispatchFormControlChangeEvent.
    bool fireOnChange = false;
    for (unsigned i = 0; i < items.size(); ++i) {
        HTMLElement* element = items[i];
        bool selected = isHTMLOptionElement(*element) && toHTMLOptionElement(element)->selected();
        if (selected != m_lastOnChangeSelection[i])
            fireOnChange = true;
        m_lastOnChangeSelection[i] = selected;
    }

    if (fireOnChange) {
        RefPtrWillBeRawPtr<HTMLSelectElement> protector(this);
        dispatchInputEvent();
        dispatchFormControlChangeEvent();
    }
}

void HTMLSelectElement::dispatchInputAndChangeEventForMenuList()
{
    ASSERT(usesMenuList());

    HTMLOptionElement* selectedOption = this->selectedOption();
    if (m_lastOnChangeOption.get() != selectedOption) {
        m_lastOnChangeOption = selectedOption;
        RefPtrWillBeRawPtr<HTMLSelectElement> protector(this);
        dispatchInputEvent();
        dispatchFormControlChangeEvent();
    }
}

void HTMLSelectElement::scrollToSelection()
{
    if (!isFinishedParsingChildren())
        return;
    if (usesMenuList())
        return;
    scrollToIndex(activeSelectionEndListIndex());
    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->listboxActiveIndexChanged(this);
}

void HTMLSelectElement::setOptionsChangedOnLayoutObject()
{
    if (LayoutObject* layoutObject = this->layoutObject()) {
        if (usesMenuList())
            toLayoutMenuList(layoutObject)->setOptionsChanged(true);
    }
}

const HTMLSelectElement::ListItems& HTMLSelectElement::listItems() const
{
    if (m_shouldRecalcListItems) {
        recalcListItems();
    } else {
#if ENABLE(ASSERT)
        WillBeHeapVector<RawPtrWillBeMember<HTMLElement>> items = m_listItems;
        recalcListItems(false);
        ASSERT(items == m_listItems);
#endif
    }

    return m_listItems;
}

void HTMLSelectElement::invalidateSelectedItems()
{
    if (HTMLCollection* collection = cachedCollection<HTMLCollection>(SelectedOptions))
        collection->invalidateCache();
}

void HTMLSelectElement::setRecalcListItems()
{
    // FIXME: This function does a bunch of confusing things depending on if it
    // is in the document or not.

    m_shouldRecalcListItems = true;
    // Manual selection anchor is reset when manipulating the select
    // programmatically.
    m_activeSelectionAnchorIndex = -1;
    m_activeSelectionEndIndex = -1;
    setOptionsChangedOnLayoutObject();
    if (!inDocument()) {
        if (HTMLOptionsCollection* collection = cachedCollection<HTMLOptionsCollection>(SelectOptions))
            collection->invalidateCache();
        invalidateSelectedItems();
    }

    if (layoutObject()) {
        if (AXObjectCache* cache = layoutObject()->document().existingAXObjectCache())
            cache->childrenChanged(this);
    }
}

void HTMLSelectElement::recalcListItems(bool updateSelectedStates) const
{
    TRACE_EVENT0("blink", "HTMLSelectElement::recalcListItems");
    m_listItems.clear();

    m_shouldRecalcListItems = false;

    HTMLOptionElement* foundSelected = nullptr;
    HTMLOptionElement* firstOption = nullptr;
    for (Element* currentElement = ElementTraversal::firstWithin(*this); currentElement && m_listItems.size() < maxListItems; ) {
        if (!currentElement->isHTMLElement()) {
            currentElement = ElementTraversal::nextSkippingChildren(*currentElement, this);
            continue;
        }
        HTMLElement& current = toHTMLElement(*currentElement);

        // We should ignore nested optgroup elements. The HTML parser flatten
        // them.  However we need to ignore nested optgroups built by DOM APIs.
        // This behavior matches to IE and Firefox.
        if (isHTMLOptGroupElement(current)) {
            if (current.parentNode() != this) {
                currentElement = ElementTraversal::nextSkippingChildren(current, this);
                continue;
            }
            m_listItems.append(&current);
            if (Element* nextElement = ElementTraversal::firstWithin(current)) {
                currentElement = nextElement;
                continue;
            }
        }

        if (isHTMLOptionElement(current)) {
            m_listItems.append(&current);

            if (updateSelectedStates && !m_multiple) {
                HTMLOptionElement& option = toHTMLOptionElement(current);
                if (!firstOption)
                    firstOption = &option;
                if (option.selected()) {
                    if (foundSelected)
                        foundSelected->setSelectedState(false);
                    foundSelected = &option;
                } else if (m_size <= 1 && !foundSelected && !option.isDisabledFormControl()) {
                    foundSelected = &option;
                    foundSelected->setSelectedState(true);
                }
            }
        }

        if (isHTMLHRElement(current))
            m_listItems.append(&current);

        // In conforming HTML code, only <optgroup> and <option> will be found
        // within a <select>. We call NodeTraversal::nextSkippingChildren so
        // that we only step into those tags that we choose to. For web-compat,
        // we should cope with the case where odd tags like a <div> have been
        // added but we handle this because such tags have already been removed
        // from the <select>'s subtree at this point.
        currentElement = ElementTraversal::nextSkippingChildren(*currentElement, this);
    }

    if (!foundSelected && m_size <= 1 && firstOption && !firstOption->selected())
        firstOption->setSelectedState(true);
}

HTMLOptionElement* HTMLSelectElement::selectedOption() const
{
    for (const auto& element : listItems()) {
        if (isHTMLOptionElement(*element) && toHTMLOptionElement(*element).selected())
            return toHTMLOptionElement(element);
    }
    return nullptr;
}

int HTMLSelectElement::selectedIndex() const
{
    unsigned index = 0;

    // Return the number of the first option selected.
    for (auto& element : listItems()) {
        if (!isHTMLOptionElement(*element))
            continue;
        if (toHTMLOptionElement(*element).selected())
            return index;
        ++index;
    }

    return -1;
}

void HTMLSelectElement::setSelectedIndex(int index)
{
    selectOption(index, DeselectOtherOptions | MakeOptionDirty);
}

int HTMLSelectElement::suggestedIndex() const
{
    return m_suggestedIndex;
}

void HTMLSelectElement::setSuggestedIndex(int suggestedIndex)
{
    m_suggestedIndex = suggestedIndex;

    if (LayoutObject* layoutObject = this->layoutObject())  {
        layoutObject->updateFromElement();
        scrollToIndex(suggestedIndex);
    }
    if (popupIsVisible())
        m_popup->updateFromElement();
}

void HTMLSelectElement::scrollToIndex(int listIndex)
{
    if (listIndex < 0)
        return;
    if (usesMenuList())
        return;
    const ListItems& items = listItems();
    int listSize = static_cast<int>(items.size());
    if (listIndex >= listSize)
        return;
    // TODO(tkent): The following isHTMLOptionElement check should be
    // unnecessary. The specified listIndex must point an HTMLOptionElement, but
    // our code about activeSelection{Anchor,End}Index is not reliable.
    if (!isHTMLOptionElement(*items[listIndex]))
        return;
    bool hasPendingTask = m_optionToScrollTo;
    // We'd like to keep an HTMLOptionElement reference rather than |listIndex|
    // because the task should work even if unselected option is inserted before
    // |listIndex| before executing scrollToIndexTask().
    m_optionToScrollTo = toHTMLOptionElement(items[listIndex]);
    if (!hasPendingTask)
        document().postTask(BLINK_FROM_HERE, createSameThreadTask(&HTMLSelectElement::scrollToIndexTask, PassRefPtrWillBeRawPtr<HTMLSelectElement>(this)));
}

void HTMLSelectElement::scrollToIndexTask()
{
    RefPtrWillBeRawPtr<HTMLOptionElement> option = m_optionToScrollTo.release();
    if (!option || !inDocument())
        return;
    // optionRemoved() makes sure m_optionToScrollTo doesn't have an option with
    // another owner.
    ASSERT(option->ownerSelectElement() == this);
    document().updateLayoutIgnorePendingStylesheets();
    if (!layoutObject() || !layoutObject()->isListBox())
        return;
    LayoutRect bounds = option->boundingBox();
    toLayoutListBox(layoutObject())->scrollToRect(bounds);
}

void HTMLSelectElement::optionSelectionStateChanged(HTMLOptionElement* option, bool optionIsSelected)
{
    ASSERT(option->ownerSelectElement() == this);
    if (optionIsSelected)
        selectOption(option->index());
    else if (!usesMenuList() || multiple())
        selectOption(-1);
    else
        selectOption(nextSelectableListIndex(-1));
}

void HTMLSelectElement::optionInserted(const HTMLOptionElement& option, bool optionIsSelected)
{
    ASSERT(option.ownerSelectElement() == this);
    setRecalcListItems();
    if (optionIsSelected)
        selectOption(option.index());
}

void HTMLSelectElement::optionRemoved(const HTMLOptionElement& option)
{
    setRecalcListItems();
    if (m_lastOnChangeOption == &option)
        m_lastOnChangeOption.clear();
    if (m_optionToScrollTo == &option)
        m_optionToScrollTo.clear();
    if (option.selected())
        setAutofilled(false);
}

// TODO(tkent): This function is not efficient.  It contains multiple O(N)
// operations. crbug.com/577989.
void HTMLSelectElement::selectOption(int optionIndex, SelectOptionFlags flags)
{
    TRACE_EVENT0("blink", "HTMLSelectElement::selectOption");
    bool shouldDeselect = !m_multiple || (flags & DeselectOtherOptions);

    const ListItems& items = listItems();
    // optionToListIndex is O(N).
    int listIndex = optionToListIndex(optionIndex);

    // selectedIndex() is O(N).
    if (isAutofilled() && selectedIndex() != optionIndex)
        setAutofilled(false);

    HTMLOptionElement* element = nullptr;
    if (listIndex >= 0) {
        // listIndex must point an HTMLOptionElement if listIndex is not -1
        // because optionToListIndex() returned it.
        element = toHTMLOptionElement(items[listIndex]);
        element->setSelectedState(true);
        if (flags & MakeOptionDirty)
            element->setDirty(true);
    }

    // deselectItemsWithoutValidation() is O(N).
    if (shouldDeselect)
        deselectItemsWithoutValidation(element);

    // We should update active selection after finishing OPTION state change
    // because setActiveSelectionAnchorIndex() stores OPTION's selection state.
    if (listIndex >= 0) {
        // setActiveSelectionAnchorIndex is O(N).
        if (m_activeSelectionAnchorIndex < 0 || shouldDeselect)
            setActiveSelectionAnchorIndex(listIndex);
        if (m_activeSelectionEndIndex < 0 || shouldDeselect)
            setActiveSelectionEndIndex(listIndex);
    }

    // For the menu list case, this is what makes the selected element appear.
    if (LayoutObject* layoutObject = this->layoutObject())
        layoutObject->updateFromElement();
    // PopupMenu::updateFromElement() posts an O(N) task.
    if (popupIsVisible())
        m_popup->updateFromElement();

    scrollToSelection();
    setNeedsValidityCheck();

    if (usesMenuList()) {
        if (flags & DispatchInputAndChangeEvent)
            dispatchInputAndChangeEventForMenuList();
        else
            m_lastOnChangeOption = element;
        if (LayoutObject* layoutObject = this->layoutObject()) {
            // Need to check usesMenuList() again because
            // dispatchInputAndChangeEventForMenuList() might change the status.
            if (usesMenuList()) {
                // didSetSelectedIndex() is O(N) because of listToOptionIndex
                // and optionToListIndex.
                toLayoutMenuList(layoutObject)->didSetSelectedIndex(listIndex);
            }
        }
    }

    notifyFormStateChanged();
}

int HTMLSelectElement::optionToListIndex(int optionIndex) const
{
    const ListItems& items = listItems();
    int listSize = static_cast<int>(items.size());
    if (optionIndex < 0 || optionIndex >= listSize)
        return -1;

    int optionIndex2 = -1;
    for (int listIndex = 0; listIndex < listSize; ++listIndex) {
        if (isHTMLOptionElement(*items[listIndex])) {
            ++optionIndex2;
            if (optionIndex2 == optionIndex)
                return listIndex;
        }
    }

    return -1;
}

int HTMLSelectElement::listToOptionIndex(int listIndex) const
{
    const ListItems& items = listItems();
    if (listIndex < 0 || listIndex >= static_cast<int>(items.size()) || !isHTMLOptionElement(*items[listIndex]))
        return -1;

    // Actual index of option not counting OPTGROUP entries that may be in list.
    int optionIndex = 0;
    for (int i = 0; i < listIndex; ++i) {
        if (isHTMLOptionElement(*items[i]))
            ++optionIndex;
    }

    return optionIndex;
}

void HTMLSelectElement::dispatchFocusEvent(Element* oldFocusedElement, WebFocusType type, InputDeviceCapabilities* sourceCapabilities)
{
    // Save the selection so it can be compared to the new selection when
    // dispatching change events during blur event dispatch.
    if (usesMenuList())
        saveLastSelection();
    HTMLFormControlElementWithState::dispatchFocusEvent(oldFocusedElement, type, sourceCapabilities);
}

void HTMLSelectElement::dispatchBlurEvent(Element* newFocusedElement, WebFocusType type, InputDeviceCapabilities* sourceCapabilities)
{
    m_typeAhead.resetSession();
    // We only need to fire change events here for menu lists, because we fire
    // change events for list boxes whenever the selection change is actually
    // made.  This matches other browsers' behavior.
    if (usesMenuList())
        dispatchInputAndChangeEventForMenuList();
    m_lastOnChangeSelection.clear();
    HTMLFormControlElementWithState::dispatchBlurEvent(newFocusedElement, type, sourceCapabilities);
}

void HTMLSelectElement::deselectItemsWithoutValidation(HTMLElement* excludeElement)
{
    for (auto& element : listItems()) {
        if (element != excludeElement && isHTMLOptionElement(*element))
            toHTMLOptionElement(element)->setSelectedState(false);
    }
}

FormControlState HTMLSelectElement::saveFormControlState() const
{
    const ListItems& items = listItems();
    size_t length = items.size();
    FormControlState state;
    for (unsigned i = 0; i < length; ++i) {
        if (!isHTMLOptionElement(*items[i]))
            continue;
        HTMLOptionElement* option = toHTMLOptionElement(items[i]);
        if (!option->selected())
            continue;
        state.append(option->value());
        state.append(String::number(i));
        if (!multiple())
            break;
    }
    return state;
}

size_t HTMLSelectElement::searchOptionsForValue(const String& value, size_t listIndexStart, size_t listIndexEnd) const
{
    const ListItems& items = listItems();
    size_t loopEndIndex = std::min(items.size(), listIndexEnd);
    for (size_t i = listIndexStart; i < loopEndIndex; ++i) {
        if (!isHTMLOptionElement(items[i]))
            continue;
        if (toHTMLOptionElement(items[i])->value() == value)
            return i;
    }
    return kNotFound;
}

void HTMLSelectElement::restoreFormControlState(const FormControlState& state)
{
    recalcListItems();

    const ListItems& items = listItems();
    size_t itemsSize = items.size();
    if (itemsSize == 0)
        return;

    for (auto& item : items) {
        if (isHTMLOptionElement(item))
            toHTMLOptionElement(item)->setSelectedState(false);
    }

    // The saved state should have at least one value and an index.
    ASSERT(state.valueSize() >= 2);
    if (!multiple()) {
        size_t index = state[1].toUInt();
        if (index < itemsSize && isHTMLOptionElement(items[index]) && toHTMLOptionElement(items[index])->value() == state[0]) {
            toHTMLOptionElement(items[index])->setSelectedState(true);
            toHTMLOptionElement(items[index])->setDirty(true);
        } else {
            size_t foundIndex = searchOptionsForValue(state[0], 0, itemsSize);
            if (foundIndex != kNotFound) {
                toHTMLOptionElement(items[foundIndex])->setSelectedState(true);
                toHTMLOptionElement(items[foundIndex])->setDirty(true);
            }
        }
    } else {
        size_t startIndex = 0;
        for (size_t i = 0; i < state.valueSize(); i+= 2) {
            const String& value = state[i];
            const size_t index = state[i + 1].toUInt();
            if (index < itemsSize && isHTMLOptionElement(items[index]) && toHTMLOptionElement(items[index])->value() == value) {
                toHTMLOptionElement(items[index])->setSelectedState(true);
                toHTMLOptionElement(items[index])->setDirty(true);
                startIndex = index + 1;
            } else {
                size_t foundIndex = searchOptionsForValue(value, startIndex, itemsSize);
                if (foundIndex == kNotFound)
                    foundIndex = searchOptionsForValue(value, 0, startIndex);
                if (foundIndex == kNotFound)
                    continue;
                toHTMLOptionElement(items[foundIndex])->setSelectedState(true);
                toHTMLOptionElement(items[foundIndex])->setDirty(true);
                startIndex = foundIndex + 1;
            }
        }
    }

    setOptionsChangedOnLayoutObject();
    setNeedsValidityCheck();
}

void HTMLSelectElement::parseMultipleAttribute(const AtomicString& value)
{
    m_multiple = !value.isNull();
    setNeedsValidityCheck();

    lazyReattachIfAttached();
}

void HTMLSelectElement::appendToFormData(FormData& formData)
{
    const AtomicString& name = this->name();
    if (name.isEmpty())
        return;

    for (auto& element : listItems()) {
        if (isHTMLOptionElement(*element) && toHTMLOptionElement(*element).selected() && !toHTMLOptionElement(*element).isDisabledFormControl())
            formData.append(name, toHTMLOptionElement(*element).value());
    }
}

void HTMLSelectElement::resetImpl()
{
    HTMLOptionElement* firstOption = nullptr;
    HTMLOptionElement* selectedOption = nullptr;

    for (auto& item : listItems()) {
        if (!isHTMLOptionElement(item))
            continue;
        HTMLOptionElement* option = toHTMLOptionElement(item);
        if (option->fastHasAttribute(selectedAttr)) {
            if (selectedOption && !m_multiple)
                selectedOption->setSelectedState(false);
            option->setSelectedState(true);
            selectedOption = option;
        } else {
            option->setSelectedState(false);
        }
        option->setDirty(false);

        if (!firstOption)
            firstOption = option;
    }

    if (!selectedOption && firstOption && !m_multiple && m_size <= 1) {
        firstOption->setSelectedState(true);
        selectedOption = firstOption;
    }

    m_lastOnChangeOption = selectedOption;
    setOptionsChangedOnLayoutObject();
    setNeedsValidityCheck();
}

void HTMLSelectElement::handlePopupOpenKeyboardEvent(Event* event)
{
    focus();
    // Calling focus() may cause us to lose our layoutObject. Return true so
    // that our caller doesn't process the event further, but don't set
    // the event as handled.
    if (!layoutObject() || !layoutObject()->isMenuList() || isDisabledFormControl())
        return;
    // Save the selection so it can be compared to the new selection when
    // dispatching change events during selectOption, which gets called from
    // valueChanged, which gets called after the user makes a selection from the
    // menu.
    saveLastSelection();
    showPopup();
    event->setDefaultHandled();
    return;
}

bool HTMLSelectElement::shouldOpenPopupForKeyDownEvent(KeyboardEvent* keyEvent)
{
    const String& keyIdentifier = keyEvent->keyIdentifier();
    LayoutTheme& layoutTheme = LayoutTheme::theme();

    if (isSpatialNavigationEnabled(document().frame()))
        return false;

    return ((layoutTheme.popsMenuByArrowKeys() &&  (keyIdentifier == "Down" || keyIdentifier == "Up"))
        || (layoutTheme.popsMenuByAltDownUpOrF4Key() && (keyIdentifier == "Down" || keyIdentifier == "Up") && keyEvent->altKey())
        || (layoutTheme.popsMenuByAltDownUpOrF4Key() && (!keyEvent->altKey() && !keyEvent->ctrlKey() && keyIdentifier == "F4")));
}

bool HTMLSelectElement::shouldOpenPopupForKeyPressEvent(KeyboardEvent *event)
{
    LayoutTheme& layoutTheme = LayoutTheme::theme();
    int keyCode = event->keyCode();

    return ((layoutTheme.popsMenuBySpaceKey() && event->keyCode() == ' ' && !m_typeAhead.hasActiveSession(event))
        || (layoutTheme.popsMenuByReturnKey() && keyCode == '\r'));
}

void HTMLSelectElement::menuListDefaultEventHandler(Event* event)
{
    if (event->type() == EventTypeNames::keydown) {
        if (!layoutObject() || !event->isKeyboardEvent())
            return;

        KeyboardEvent* keyEvent = toKeyboardEvent(event);
        if (shouldOpenPopupForKeyDownEvent(keyEvent)) {
            handlePopupOpenKeyboardEvent(event);
            return;
        }

        // When using spatial navigation, we want to be able to navigate away
        // from the select element when the user hits any of the arrow keys,
        // instead of changing the selection.
        if (isSpatialNavigationEnabled(document().frame())) {
            if (!m_activeSelectionState)
                return;
        }

        // The key handling below shouldn't be used for non spatial navigation
        // mode Mac
        if (LayoutTheme::theme().popsMenuByArrowKeys() && !isSpatialNavigationEnabled(document().frame()))
            return;

        const String& keyIdentifier = keyEvent->keyIdentifier();
        bool handled = true;
        const ListItems& listItems = this->listItems();
        int listIndex = optionToListIndex(selectedIndex());

        if (keyIdentifier == "Down" || keyIdentifier == "Right")
            listIndex = nextValidIndex(listIndex, SkipForwards, 1);
        else if (keyIdentifier == "Up" || keyIdentifier == "Left")
            listIndex = nextValidIndex(listIndex, SkipBackwards, 1);
        else if (keyIdentifier == "PageDown")
            listIndex = nextValidIndex(listIndex, SkipForwards, 3);
        else if (keyIdentifier == "PageUp")
            listIndex = nextValidIndex(listIndex, SkipBackwards, 3);
        else if (keyIdentifier == "Home")
            listIndex = nextValidIndex(-1, SkipForwards, 1);
        else if (keyIdentifier == "End")
            listIndex = nextValidIndex(listItems.size(), SkipBackwards, 1);
        else
            handled = false;

        if (handled && static_cast<size_t>(listIndex) < listItems.size())
            selectOption(listToOptionIndex(listIndex), DeselectOtherOptions | MakeOptionDirty | DispatchInputAndChangeEvent);

        if (handled)
            event->setDefaultHandled();
    }

    if (event->type() == EventTypeNames::keypress) {
        if (!layoutObject() || !event->isKeyboardEvent())
            return;

        int keyCode = toKeyboardEvent(event)->keyCode();
        if (keyCode == ' ' && isSpatialNavigationEnabled(document().frame())) {
            // Use space to toggle arrow key handling for selection change or
            // spatial navigation.
            m_activeSelectionState = !m_activeSelectionState;
            event->setDefaultHandled();
            return;
        }

        KeyboardEvent* keyEvent = toKeyboardEvent(event);
        if (shouldOpenPopupForKeyPressEvent(keyEvent)) {
            handlePopupOpenKeyboardEvent(event);
            return;
        }

        if (!LayoutTheme::theme().popsMenuByReturnKey() && keyCode == '\r') {
            if (form())
                form()->submitImplicitly(event, false);
            dispatchInputAndChangeEventForMenuList();
            event->setDefaultHandled();
        }
    }

    if (event->type() == EventTypeNames::mousedown && event->isMouseEvent() && toMouseEvent(event)->button() == LeftButton) {
        InputDeviceCapabilities* sourceCapabilities = toMouseEvent(event)->fromTouch() ? InputDeviceCapabilities::firesTouchEventsSourceCapabilities() : InputDeviceCapabilities::doesntFireTouchEventsSourceCapabilities();
        focus(FocusParams(SelectionBehaviorOnFocus::Restore, WebFocusTypeNone, sourceCapabilities));
        if (layoutObject() && layoutObject()->isMenuList() && !isDisabledFormControl()) {
            if (popupIsVisible()) {
                hidePopup();
            } else {
                // Save the selection so it can be compared to the new selection
                // when we call onChange during selectOption, which gets called
                // from valueChanged, which gets called after the user makes a
                // selection from the menu.
                saveLastSelection();
                // TODO(lanwei): Will check if we need to add
                // InputDeviceCapabilities here when select menu list gets
                // focus, see https://crbug.com/476530.
                showPopup();
            }
        }
        event->setDefaultHandled();
    }

    if (event->type() == EventTypeNames::blur) {
        if (popupIsVisible())
            hidePopup();
    }
}

void HTMLSelectElement::updateSelectedState(int listIndex, bool multi, bool shift)
{
    ASSERT(listIndex >= 0);

    HTMLElement* clickedElement = listItems()[listIndex];
    ASSERT(clickedElement);
    if (isHTMLOptGroupElement(clickedElement))
        return;

    // Save the selection so it can be compared to the new selection when
    // dispatching change events during mouseup, or after autoscroll finishes.
    saveLastSelection();

    m_activeSelectionState = true;

    bool shiftSelect = m_multiple && shift;
    bool multiSelect = m_multiple && multi && !shift;

    if (isHTMLOptionElement(*clickedElement)) {
        HTMLOptionElement& option = toHTMLOptionElement(*clickedElement);
        // Keep track of whether an active selection (like during drag
        // selection), should select or deselect.
        if (option.selected() && multiSelect) {
            m_activeSelectionState = false;
            option.setSelectedState(false);
            option.setDirty(true);
        }
    }

    // If we're not in any special multiple selection mode, then deselect all
    // other items, excluding the clicked option. If no option was clicked, then
    // this will deselect all items in the list.
    if (!shiftSelect && !multiSelect)
        deselectItemsWithoutValidation(clickedElement);

    // If the anchor hasn't been set, and we're doing a single selection or a
    // shift selection, then initialize the anchor to the first selected index.
    if (m_activeSelectionAnchorIndex < 0 && !multiSelect)
        setActiveSelectionAnchorIndex(selectedIndex());

    // Set the selection state of the clicked option.
    if (isHTMLOptionElement(*clickedElement) && !toHTMLOptionElement(*clickedElement).isDisabledFormControl()) {
        toHTMLOptionElement(*clickedElement).setSelectedState(true);
        toHTMLOptionElement(*clickedElement).setDirty(true);
    }

    // If there was no selectedIndex() for the previous initialization, or If
    // we're doing a single selection, or a multiple selection (using cmd or
    // ctrl), then initialize the anchor index to the listIndex that just got
    // clicked.
    if (m_activeSelectionAnchorIndex < 0 || !shiftSelect)
        setActiveSelectionAnchorIndex(listIndex);

    setActiveSelectionEndIndex(listIndex);
    updateListBoxSelection(!multiSelect);
}

int HTMLSelectElement::listIndexForEventTargetOption(const Event& event)
{
    Node* targetNode = event.target()->toNode();
    if (!targetNode || !isHTMLOptionElement(*targetNode))
        return -1;
    return listIndexForOption(toHTMLOptionElement(*targetNode));
}

int HTMLSelectElement::listIndexForOption(const HTMLOptionElement& option)
{
    const ListItems& items = this->listItems();
    size_t length = items.size();
    for (size_t i = 0; i < length; ++i) {
        if (items[i].get() == &option)
            return i;
    }
    return -1;
}

AutoscrollController* HTMLSelectElement::autoscrollController() const
{
    if (Page* page = document().page())
        return &page->autoscrollController();
    return nullptr;
}

void HTMLSelectElement::handleMouseRelease()
{
    // We didn't start this click/drag on any options.
    if (m_lastOnChangeSelection.isEmpty())
        return;
    listBoxOnChange();
}

void HTMLSelectElement::listBoxDefaultEventHandler(Event* event)
{
    const ListItems& listItems = this->listItems();
    if (event->type() == EventTypeNames::gesturetap && event->isGestureEvent()) {
        focus();
        // Calling focus() may cause us to lose our layoutObject or change the
        // layoutObject type, in which case do not want to handle the event.
        if (!layoutObject() || !layoutObject()->isListBox())
            return;

        // Convert to coords relative to the list box if needed.
        GestureEvent& gestureEvent = toGestureEvent(*event);
        int listIndex = listIndexForEventTargetOption(gestureEvent);
        if (listIndex >= 0) {
            if (!isDisabledFormControl()) {
                updateSelectedState(listIndex, true, gestureEvent.shiftKey());
                listBoxOnChange();
            }
            event->setDefaultHandled();
        }

    } else if (event->type() == EventTypeNames::mousedown && event->isMouseEvent() && toMouseEvent(event)->button() == LeftButton) {
        focus();
        // Calling focus() may cause us to lose our layoutObject, in which case
        // do not want to handle the event.
        if (!layoutObject() || !layoutObject()->isListBox() || isDisabledFormControl())
            return;

        // Convert to coords relative to the list box if needed.
        MouseEvent* mouseEvent = toMouseEvent(event);
        int listIndex = listIndexForEventTargetOption(*mouseEvent);
        if (listIndex >= 0) {
            if (!isDisabledFormControl()) {
#if OS(MACOSX)
                updateSelectedState(listIndex, mouseEvent->metaKey(), mouseEvent->shiftKey());
#else
                updateSelectedState(listIndex, mouseEvent->ctrlKey(), mouseEvent->shiftKey());
#endif
            }
            if (LocalFrame* frame = document().frame())
                frame->eventHandler().setMouseDownMayStartAutoscroll();

            event->setDefaultHandled();
        }

    } else if (event->type() == EventTypeNames::mousemove && event->isMouseEvent()) {
        MouseEvent* mouseEvent = toMouseEvent(event);
        if (mouseEvent->button() != LeftButton || !mouseEvent->buttonDown())
            return;

        if (Page* page = document().page())
            page->autoscrollController().startAutoscrollForSelection(layoutObject());
        // Mousedown didn't happen in this element.
        if (m_lastOnChangeSelection.isEmpty())
            return;

        int listIndex = listIndexForEventTargetOption(*mouseEvent);
        if (listIndex >= 0) {
            if (!isDisabledFormControl()) {
                if (m_multiple) {
                    // Only extend selection if there is something selected.
                    if (m_activeSelectionAnchorIndex < 0)
                        return;

                    setActiveSelectionEndIndex(listIndex);
                    updateListBoxSelection(false);
                } else {
                    setActiveSelectionAnchorIndex(listIndex);
                    setActiveSelectionEndIndex(listIndex);
                    updateListBoxSelection(true);
                }
            }
        }

    } else if (event->type() == EventTypeNames::mouseup && event->isMouseEvent() && toMouseEvent(event)->button() == LeftButton && layoutObject()) {
        if (document().page() && document().page()->autoscrollController().autoscrollInProgress(toLayoutBox(layoutObject())))
            document().page()->autoscrollController().stopAutoscroll();
        else
            handleMouseRelease();

    } else if (event->type() == EventTypeNames::keydown) {
        if (!event->isKeyboardEvent())
            return;
        const String& keyIdentifier = toKeyboardEvent(event)->keyIdentifier();

        bool handled = false;
        int endIndex = 0;
        if (m_activeSelectionEndIndex < 0) {
            // Initialize the end index
            if (keyIdentifier == "Down" || keyIdentifier == "PageDown") {
                int startIndex = lastSelectedListIndex();
                handled = true;
                if (keyIdentifier == "Down")
                    endIndex = nextSelectableListIndex(startIndex);
                else
                    endIndex = nextSelectableListIndexPageAway(startIndex, SkipForwards);
            } else if (keyIdentifier == "Up" || keyIdentifier == "PageUp") {
                int startIndex = optionToListIndex(selectedIndex());
                handled = true;
                if (keyIdentifier == "Up")
                    endIndex = previousSelectableListIndex(startIndex);
                else
                    endIndex = nextSelectableListIndexPageAway(startIndex, SkipBackwards);
            }
        } else {
            // Set the end index based on the current end index.
            if (keyIdentifier == "Down") {
                endIndex = nextSelectableListIndex(m_activeSelectionEndIndex);
                handled = true;
            } else if (keyIdentifier == "Up") {
                endIndex = previousSelectableListIndex(m_activeSelectionEndIndex);
                handled = true;
            } else if (keyIdentifier == "PageDown") {
                endIndex = nextSelectableListIndexPageAway(m_activeSelectionEndIndex, SkipForwards);
                handled = true;
            } else if (keyIdentifier == "PageUp") {
                endIndex = nextSelectableListIndexPageAway(m_activeSelectionEndIndex, SkipBackwards);
                handled = true;
            }
        }
        if (keyIdentifier == "Home") {
            endIndex = firstSelectableListIndex();
            handled = true;
        } else if (keyIdentifier == "End") {
            endIndex = lastSelectableListIndex();
            handled = true;
        }

        if (isSpatialNavigationEnabled(document().frame())) {
            // Check if the selection moves to the boundary.
            if (keyIdentifier == "Left" || keyIdentifier == "Right" || ((keyIdentifier == "Down" || keyIdentifier == "Up") && endIndex == m_activeSelectionEndIndex))
                return;
        }

        if (endIndex >= 0 && handled) {
            // Save the selection so it can be compared to the new selection
            // when dispatching change events immediately after making the new
            // selection.
            saveLastSelection();

            ASSERT_UNUSED(listItems, !listItems.size() || static_cast<size_t>(endIndex) < listItems.size());
            setActiveSelectionEndIndex(endIndex);

            bool selectNewItem = !m_multiple || toKeyboardEvent(event)->shiftKey() || !isSpatialNavigationEnabled(document().frame());
            if (selectNewItem)
                m_activeSelectionState = true;
            // If the anchor is unitialized, or if we're going to deselect all
            // other options, then set the anchor index equal to the end index.
            bool deselectOthers = !m_multiple || (!toKeyboardEvent(event)->shiftKey() && selectNewItem);
            if (m_activeSelectionAnchorIndex < 0 || deselectOthers) {
                if (deselectOthers)
                    deselectItemsWithoutValidation();
                setActiveSelectionAnchorIndex(m_activeSelectionEndIndex);
            }

            scrollToIndex(endIndex);
            if (selectNewItem) {
                updateListBoxSelection(deselectOthers);
                listBoxOnChange();
            } else {
                scrollToSelection();
            }

            event->setDefaultHandled();
        }

    } else if (event->type() == EventTypeNames::keypress) {
        if (!event->isKeyboardEvent())
            return;
        int keyCode = toKeyboardEvent(event)->keyCode();

        if (keyCode == '\r') {
            if (form())
                form()->submitImplicitly(event, false);
            event->setDefaultHandled();
        } else if (m_multiple && keyCode == ' ' && isSpatialNavigationEnabled(document().frame())) {
            // Use space to toggle selection change.
            m_activeSelectionState = !m_activeSelectionState;
            updateSelectedState(listToOptionIndex(m_activeSelectionEndIndex), true /*multi*/, false /*shift*/);
            listBoxOnChange();
            event->setDefaultHandled();
        }
    }
}

void HTMLSelectElement::defaultEventHandler(Event* event)
{
    if (!layoutObject())
        return;

    if (isDisabledFormControl()) {
        HTMLFormControlElementWithState::defaultEventHandler(event);
        return;
    }

    if (usesMenuList())
        menuListDefaultEventHandler(event);
    else
        listBoxDefaultEventHandler(event);
    if (event->defaultHandled())
        return;

    if (event->type() == EventTypeNames::keypress && event->isKeyboardEvent()) {
        KeyboardEvent* keyboardEvent = toKeyboardEvent(event);
        if (!keyboardEvent->ctrlKey() && !keyboardEvent->altKey() && !keyboardEvent->metaKey() && isPrintableChar(keyboardEvent->charCode())) {
            typeAheadFind(keyboardEvent);
            event->setDefaultHandled();
            return;
        }
    }
    HTMLFormControlElementWithState::defaultEventHandler(event);
}

int HTMLSelectElement::lastSelectedListIndex() const
{
    const ListItems& items = listItems();
    for (size_t i = items.size(); i;) {
        HTMLElement* element = items[--i];
        if (isHTMLOptionElement(*element) && toHTMLOptionElement(element)->selected())
            return i;
    }
    return -1;
}

int HTMLSelectElement::indexOfSelectedOption() const
{
    return optionToListIndex(selectedIndex());
}

int HTMLSelectElement::optionCount() const
{
    return listItems().size();
}

String HTMLSelectElement::optionAtIndex(int index) const
{
    const ListItems& items = listItems();
    HTMLElement* element = items[index];
    if (!isHTMLOptionElement(*element) || toHTMLOptionElement(element)->isDisabledFormControl())
        return String();
    return toHTMLOptionElement(element)->displayLabel();
}

void HTMLSelectElement::typeAheadFind(KeyboardEvent* event)
{
    int index = m_typeAhead.handleEvent(event, TypeAhead::MatchPrefix | TypeAhead::CycleFirstChar);
    if (index < 0)
        return;
    selectOption(listToOptionIndex(index), DeselectOtherOptions | MakeOptionDirty | DispatchInputAndChangeEvent);
    if (!usesMenuList())
        listBoxOnChange();
}

void HTMLSelectElement::accessKeySetSelectedIndex(int index)
{
    // First bring into focus the list box.
    if (!focused())
        accessKeyAction(false);

    const ListItems& items = listItems();
    int listIndex = optionToListIndex(index);
    if (listIndex < 0)
        return;
    HTMLElement& element = *items[listIndex];
    if (!isHTMLOptionElement(element))
        return;
    EventQueueScope scope;
    // If this index is already selected, unselect. otherwise update the
    // selected index.
    if (toHTMLOptionElement(element).selected()) {
        if (usesMenuList())
            selectOption(-1, DispatchInputAndChangeEvent);
        else
            toHTMLOptionElement(element).setSelectedState(false);
    } else {
        selectOption(index, DispatchInputAndChangeEvent);
    }
    toHTMLOptionElement(element).setDirty(true);
    if (usesMenuList())
        return;
    listBoxOnChange();
    scrollToSelection();
}

unsigned HTMLSelectElement::length() const
{
    unsigned options = 0;
    for (auto& item : listItems()) {
        if (isHTMLOptionElement(*item))
            ++options;
    }
    return options;
}

void HTMLSelectElement::finishParsingChildren()
{
    HTMLFormControlElementWithState::finishParsingChildren();
    updateListItemSelectedStates();
    if (usesMenuList())
        return;
    scrollToIndex(optionToListIndex(selectedIndex()));
    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->listboxActiveIndexChanged(this);
}

bool HTMLSelectElement::anonymousIndexedSetter(unsigned index, PassRefPtrWillBeRawPtr<HTMLOptionElement> value, ExceptionState& exceptionState)
{
    if (!value) { // undefined or null
        remove(index);
        return true;
    }
    setOption(index, value.get(), exceptionState);
    return true;
}

bool HTMLSelectElement::isInteractiveContent() const
{
    return true;
}

bool HTMLSelectElement::supportsAutofocus() const
{
    return true;
}

void HTMLSelectElement::updateListOnLayoutObject()
{
    setOptionsChangedOnLayoutObject();
}

DEFINE_TRACE(HTMLSelectElement)
{
#if ENABLE(OILPAN)
    visitor->trace(m_listItems);
#endif
    visitor->trace(m_lastOnChangeOption);
    visitor->trace(m_optionToScrollTo);
    visitor->trace(m_popup);
    HTMLFormControlElementWithState::trace(visitor);
}

void HTMLSelectElement::willRecalcStyle(StyleRecalcChange change)
{
    // recalcListItems will update the selected state of the <option> elements
    // in this <select> so we need to do it before we recalc their style so they
    // match the right selectors (ex. :checked).
    // TODO(esprehn): Find a way to avoid needing a willRecalcStyle callback.
    if (m_shouldRecalcListItems)
        recalcListItems();
}

void HTMLSelectElement::didAddUserAgentShadowRoot(ShadowRoot& root)
{
    RefPtrWillBeRawPtr<HTMLContentElement> content = HTMLContentElement::create(document());
    content->setAttribute(selectAttr, "option,optgroup,hr");
    root.appendChild(content);
}

HTMLOptionElement* HTMLSelectElement::spatialNavigationFocusedOption()
{
    if (!isSpatialNavigationEnabled(document().frame()))
        return nullptr;
    int focusedIndex = activeSelectionEndListIndex();
    if (focusedIndex < 0)
        focusedIndex = firstSelectableListIndex();
    if (focusedIndex < 0)
        return nullptr;
    HTMLElement* focused = listItems()[focusedIndex];
    return isHTMLOptionElement(focused) ? toHTMLOptionElement(focused) : nullptr;
}

String HTMLSelectElement::itemText(const Element& element) const
{
    String itemString;
    if (isHTMLOptGroupElement(element))
        itemString = toHTMLOptGroupElement(element).groupLabelText();
    else if (isHTMLOptionElement(element))
        itemString = toHTMLOptionElement(element).textIndentedToRespectGroupLabel();

    if (layoutObject())
        applyTextTransform(layoutObject()->style(), itemString, ' ');
    return itemString;
}

bool HTMLSelectElement::itemIsDisplayNone(Element& element) const
{
    if (isHTMLOptionElement(element))
        return toHTMLOptionElement(element).isDisplayNone();
    if (const ComputedStyle* style = itemComputedStyle(element))
        return style->display() == NONE;
    return false;
}

const ComputedStyle* HTMLSelectElement::itemComputedStyle(Element& element) const
{
    return element.computedStyle() ? element.computedStyle() : element.ensureComputedStyle();
}

IntRect HTMLSelectElement::elementRectRelativeToViewport() const
{
    if (!layoutObject())
        return IntRect();
    // We don't use absoluteBoundingBoxRect() because it can return an IntRect
    // larger the actual size by 1px.
    return document().view()->contentsToViewport(roundedIntRect(layoutObject()->absoluteBoundingBoxFloatRect()));
}

LayoutUnit HTMLSelectElement::clientPaddingLeft() const
{
    if (layoutObject() && layoutObject()->isMenuList())
        return toLayoutMenuList(layoutObject())->clientPaddingLeft();
    return 0;
}

LayoutUnit HTMLSelectElement::clientPaddingRight() const
{
    if (layoutObject() && layoutObject()->isMenuList())
        return toLayoutMenuList(layoutObject())->clientPaddingRight();
    return 0;
}

void HTMLSelectElement::popupDidHide()
{
    m_popupIsVisible = false;
    if (AXObjectCache* cache = document().existingAXObjectCache()) {
        if (layoutObject() && layoutObject()->isMenuList())
            cache->didHideMenuListPopup(toLayoutMenuList(layoutObject()));
    }
}

void HTMLSelectElement::setIndexToSelectOnCancel(int listIndex)
{
    m_indexToSelectOnCancel = listIndex;
    if (layoutObject())
        layoutObject()->updateFromElement();
}

int HTMLSelectElement::optionIndexToBeShown() const
{
    if (m_indexToSelectOnCancel >= 0)
        return listToOptionIndex(m_indexToSelectOnCancel);
    if (suggestedIndex() >= 0)
        return suggestedIndex();
    return selectedIndex();
}

void HTMLSelectElement::valueChanged(unsigned listIndex)
{
    // Check to ensure a page navigation has not occurred while the popup was
    // up.
    Document& doc = document();
    if (&doc != doc.frame()->document())
        return;

    setIndexToSelectOnCancel(-1);
    optionSelectedByUser(listToOptionIndex(listIndex), true);
}

void HTMLSelectElement::popupDidCancel()
{
    if (m_indexToSelectOnCancel >= 0)
        valueChanged(m_indexToSelectOnCancel);
}

void HTMLSelectElement::provisionalSelectionChanged(unsigned listIndex)
{
    setIndexToSelectOnCancel(listIndex);
}

void HTMLSelectElement::showPopup()
{
    if (popupIsVisible())
        return;
    if (document().frameHost()->chromeClient().hasOpenedPopup())
        return;
    if (!layoutObject() || !layoutObject()->isMenuList())
        return;

    if (!m_popup)
        m_popup = document().frameHost()->chromeClient().openPopupMenu(*document().frame(), *this);
    m_popupIsVisible = true;

    LayoutMenuList* menuList = toLayoutMenuList(layoutObject());
    FloatQuad quad(menuList->localToAbsoluteQuad(FloatQuad(menuList->borderBoundingBox())));
    IntSize size = pixelSnappedIntRect(menuList->frameRect()).size();
    m_popup->show(quad, size, optionToListIndex(selectedIndex()));
    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->didShowMenuListPopup(menuList);
}

void HTMLSelectElement::hidePopup()
{
    if (m_popup)
        m_popup->hide();
}

void HTMLSelectElement::didRecalcStyle(StyleRecalcChange change)
{
    HTMLFormControlElementWithState::didRecalcStyle(change);
    if (popupIsVisible())
        m_popup->updateFromElement();
}

void HTMLSelectElement::detach(const AttachContext& context)
{
    HTMLFormControlElementWithState::detach(context);
    if (m_popup)
        m_popup->disconnectClient();
    m_popupIsVisible = false;
    m_popup = nullptr;
}

void HTMLSelectElement::resetTypeAheadSessionForTesting()
{
    m_typeAhead.resetSession();
}

} // namespace blink
