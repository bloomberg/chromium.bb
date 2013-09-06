/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebAXObject.h"

#include "HTMLNames.h"
#include "WebDocument.h"
#include "WebNode.h"
#include "core/accessibility/AXObjectCache.h"
#include "core/accessibility/AccessibilityObject.h"
#include "core/accessibility/AccessibilityTable.h"
#include "core/accessibility/AccessibilityTableCell.h"
#include "core/accessibility/AccessibilityTableColumn.h"
#include "core/accessibility/AccessibilityTableRow.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/dom/Document.h"
#include "core/dom/Node.h"
#include "core/page/EventHandler.h"
#include "core/page/FrameView.h"
#include "core/platform/PlatformKeyboardEvent.h"
#include "core/rendering/style/RenderStyle.h"
#include "public/platform/WebPoint.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "wtf/text/StringBuilder.h"

using namespace WebCore;

namespace WebKit {

void WebAXObject::reset()
{
    m_private.reset();
}

void WebAXObject::assign(const WebKit::WebAXObject& other)
{
    m_private = other.m_private;
}

bool WebAXObject::equals(const WebAXObject& n) const
{
    return m_private.get() == n.m_private.get();
}

// static
void WebAXObject::enableAccessibility()
{
    AXObjectCache::enableAccessibility();
}

// static
bool WebAXObject::accessibilityEnabled()
{
    return AXObjectCache::accessibilityEnabled();
}

void WebAXObject::startCachingComputedObjectAttributesUntilTreeMutates()
{
    m_private->axObjectCache()->startCachingComputedObjectAttributesUntilTreeMutates();
}

void WebAXObject::stopCachingComputedObjectAttributes()
{
    m_private->axObjectCache()->stopCachingComputedObjectAttributes();
}

bool WebAXObject::isDetached() const
{
    if (m_private.isNull())
        return true;

    return m_private->isDetached();
}

int WebAXObject::axID() const
{
    if (isDetached())
        return -1;

    return m_private->axObjectID();
}

bool WebAXObject::updateBackingStoreAndCheckValidity()
{
    if (!isDetached())
        m_private->updateBackingStore();
    return !isDetached();
}

WebString WebAXObject::accessibilityDescription() const
{
    if (isDetached())
        return WebString();

    return m_private->accessibilityDescription();
}

WebString WebAXObject::actionVerb() const
{
    if (isDetached())
        return WebString();

    return m_private->actionVerb();
}

bool WebAXObject::canDecrement() const
{
    if (isDetached())
        return false;

    return m_private->isSlider();
}

bool WebAXObject::canIncrement() const
{
    if (isDetached())
        return false;

    return m_private->isSlider();
}

bool WebAXObject::canPress() const
{
    if (isDetached())
        return false;

    return m_private->actionElement() || m_private->isButton() || m_private->isMenuRelated();
}

bool WebAXObject::canSetFocusAttribute() const
{
    if (isDetached())
        return false;

    return m_private->canSetFocusAttribute();
}

bool WebAXObject::canSetValueAttribute() const
{
    if (isDetached())
        return false;

    return m_private->canSetValueAttribute();
}

unsigned WebAXObject::childCount() const
{
    if (isDetached())
        return 0;

    return m_private->children().size();
}

WebAXObject WebAXObject::childAt(unsigned index) const
{
    if (isDetached())
        return WebAXObject();

    if (m_private->children().size() <= index)
        return WebAXObject();

    return WebAXObject(m_private->children()[index]);
}

WebAXObject WebAXObject::parentObject() const
{
    if (isDetached())
        return WebAXObject();

    return WebAXObject(m_private->parentObject());
}

bool WebAXObject::canSetSelectedAttribute() const
{
    if (isDetached())
        return 0;

    return m_private->canSetSelectedAttribute();
}

bool WebAXObject::isAnchor() const
{
    if (isDetached())
        return 0;

    return m_private->isAnchor();
}

bool WebAXObject::isAriaReadOnly() const
{
    if (isDetached())
        return 0;

    return equalIgnoringCase(m_private->getAttribute(HTMLNames::aria_readonlyAttr), "true");
}

bool WebAXObject::isButtonStateMixed() const
{
    if (isDetached())
        return 0;

    return m_private->checkboxOrRadioValue() == ButtonStateMixed;
}

bool WebAXObject::isChecked() const
{
    if (isDetached())
        return 0;

    return m_private->isChecked();
}

bool WebAXObject::isClickable() const
{
    if (isDetached())
        return 0;

    return m_private->isClickable();
}

bool WebAXObject::isCollapsed() const
{
    if (isDetached())
        return 0;

    return m_private->isCollapsed();
}

bool WebAXObject::isControl() const
{
    if (isDetached())
        return 0;

    return m_private->isControl();
}

bool WebAXObject::isEnabled() const
{
    if (isDetached())
        return 0;

    return m_private->isEnabled();
}

bool WebAXObject::isFocused() const
{
    if (isDetached())
        return 0;

    return m_private->isFocused();
}

bool WebAXObject::isHovered() const
{
    if (isDetached())
        return 0;

    return m_private->isHovered();
}

bool WebAXObject::isIndeterminate() const
{
    if (isDetached())
        return 0;

    return m_private->isIndeterminate();
}

bool WebAXObject::isLinked() const
{
    if (isDetached())
        return 0;

    return m_private->isLinked();
}

bool WebAXObject::isLoaded() const
{
    if (isDetached())
        return 0;

    return m_private->isLoaded();
}

bool WebAXObject::isMultiSelectable() const
{
    if (isDetached())
        return 0;

    return m_private->isMultiSelectable();
}

bool WebAXObject::isOffScreen() const
{
    if (isDetached())
        return 0;

    return m_private->isOffScreen();
}

bool WebAXObject::isPasswordField() const
{
    if (isDetached())
        return 0;

    return m_private->isPasswordField();
}

bool WebAXObject::isPressed() const
{
    if (isDetached())
        return 0;

    return m_private->isPressed();
}

bool WebAXObject::isReadOnly() const
{
    if (isDetached())
        return 0;

    return m_private->isReadOnly();
}

bool WebAXObject::isRequired() const
{
    if (isDetached())
        return 0;

    return m_private->isRequired();
}

bool WebAXObject::isSelected() const
{
    if (isDetached())
        return 0;

    return m_private->isSelected();
}

bool WebAXObject::isSelectedOptionActive() const
{
    if (isDetached())
        return false;

    return m_private->isSelectedOptionActive();
}

bool WebAXObject::isVertical() const
{
    if (isDetached())
        return 0;

    return m_private->orientation() == AccessibilityOrientationVertical;
}

bool WebAXObject::isVisible() const
{
    if (isDetached())
        return 0;

    return m_private->isVisible();
}

bool WebAXObject::isVisited() const
{
    if (isDetached())
        return 0;

    return m_private->isVisited();
}

WebString WebAXObject::accessKey() const
{
    if (isDetached())
        return WebString();

    return WebString(m_private->accessKey());
}

bool WebAXObject::ariaHasPopup() const
{
    if (isDetached())
        return 0;

    return m_private->ariaHasPopup();
}

bool WebAXObject::ariaLiveRegionAtomic() const
{
    if (isDetached())
        return 0;

    return m_private->ariaLiveRegionAtomic();
}

bool WebAXObject::ariaLiveRegionBusy() const
{
    if (isDetached())
        return 0;

    return m_private->ariaLiveRegionBusy();
}

WebString WebAXObject::ariaLiveRegionRelevant() const
{
    if (isDetached())
        return WebString();

    return m_private->ariaLiveRegionRelevant();
}

WebString WebAXObject::ariaLiveRegionStatus() const
{
    if (isDetached())
        return WebString();

    return m_private->ariaLiveRegionStatus();
}

WebRect WebAXObject::boundingBoxRect() const
{
    if (isDetached())
        return WebRect();

    return pixelSnappedIntRect(m_private->elementRect());
}

bool WebAXObject::canvasHasFallbackContent() const
{
    if (isDetached())
        return false;

    return m_private->canvasHasFallbackContent();
}

WebPoint WebAXObject::clickPoint() const
{
    if (isDetached())
        return WebPoint();

    return WebPoint(m_private->clickPoint());
}

void WebAXObject::colorValue(int& r, int& g, int& b) const
{
    if (isDetached())
        return;

    m_private->colorValue(r, g, b);
}

double WebAXObject::estimatedLoadingProgress() const
{
    if (isDetached())
        return 0.0;

    return m_private->estimatedLoadingProgress();
}

WebString WebAXObject::helpText() const
{
    if (isDetached())
        return WebString();

    return m_private->helpText();
}

int WebAXObject::headingLevel() const
{
    if (isDetached())
        return 0;

    return m_private->headingLevel();
}

int WebAXObject::hierarchicalLevel() const
{
    if (isDetached())
        return 0;

    return m_private->hierarchicalLevel();
}

WebAXObject WebAXObject::hitTest(const WebPoint& point) const
{
    if (isDetached())
        return WebAXObject();

    IntPoint contentsPoint = m_private->documentFrameView()->windowToContents(point);
    RefPtr<AccessibilityObject> hit = m_private->accessibilityHitTest(contentsPoint);

    if (hit)
        return WebAXObject(hit);

    if (m_private->elementRect().contains(contentsPoint))
        return *this;

    return WebAXObject();
}

WebString WebAXObject::keyboardShortcut() const
{
    if (isDetached())
        return WebString();

    String accessKey = m_private->accessKey();
    if (accessKey.isNull())
        return WebString();

    DEFINE_STATIC_LOCAL(String, modifierString, ());
    if (modifierString.isNull()) {
        unsigned modifiers = EventHandler::accessKeyModifiers();
        // Follow the same order as Mozilla MSAA implementation:
        // Ctrl+Alt+Shift+Meta+key. MSDN states that keyboard shortcut strings
        // should not be localized and defines the separator as "+".
        StringBuilder modifierStringBuilder;
        if (modifiers & PlatformEvent::CtrlKey)
            modifierStringBuilder.appendLiteral("Ctrl+");
        if (modifiers & PlatformEvent::AltKey)
            modifierStringBuilder.appendLiteral("Alt+");
        if (modifiers & PlatformEvent::ShiftKey)
            modifierStringBuilder.appendLiteral("Shift+");
        if (modifiers & PlatformEvent::MetaKey)
            modifierStringBuilder.appendLiteral("Win+");
        modifierString = modifierStringBuilder.toString();
    }

    return String(modifierString + accessKey);
}

bool WebAXObject::performDefaultAction() const
{
    if (isDetached())
        return false;

    return m_private->performDefaultAction();
}

bool WebAXObject::increment() const
{
    if (isDetached())
        return false;

    if (canIncrement()) {
        m_private->increment();
        return true;
    }
    return false;
}

bool WebAXObject::decrement() const
{
    if (isDetached())
        return false;

    if (canDecrement()) {
        m_private->decrement();
        return true;
    }
    return false;
}

bool WebAXObject::press() const
{
    if (isDetached())
        return false;

    return m_private->press();
}

WebAXRole WebAXObject::role() const
{
    if (isDetached())
        return WebKit::WebAXRoleUnknown;

    return static_cast<WebAXRole>(m_private->roleValue());
}

unsigned WebAXObject::selectionEnd() const
{
    if (isDetached())
        return 0;

    return m_private->selectedTextRange().start + m_private->selectedTextRange().length;
}

unsigned WebAXObject::selectionStart() const
{
    if (isDetached())
        return 0;

    return m_private->selectedTextRange().start;
}

unsigned WebAXObject::selectionEndLineNumber() const
{
    if (isDetached())
        return 0;

    VisiblePosition position = m_private->visiblePositionForIndex(selectionEnd());
    int lineNumber = m_private->lineForPosition(position);
    if (lineNumber < 0)
        return 0;
    return lineNumber;
}

unsigned WebAXObject::selectionStartLineNumber() const
{
    if (isDetached())
        return 0;

    VisiblePosition position = m_private->visiblePositionForIndex(selectionStart());
    int lineNumber = m_private->lineForPosition(position);
    if (lineNumber < 0)
        return 0;
    return lineNumber;
}

void WebAXObject::setFocused(bool on) const
{
    if (!isDetached())
        m_private->setFocused(on);
}

void WebAXObject::setSelectedTextRange(int selectionStart, int selectionEnd) const
{
    if (isDetached())
        return;

    m_private->setSelectedTextRange(PlainTextRange(selectionStart, selectionEnd - selectionStart));
}

WebString WebAXObject::stringValue() const
{
    if (isDetached())
        return WebString();

    return m_private->stringValue();
}

WebString WebAXObject::title() const
{
    if (isDetached())
        return WebString();

    return m_private->title();
}

WebAXObject WebAXObject::titleUIElement() const
{
    if (isDetached())
        return WebAXObject();

    if (!m_private->exposesTitleUIElement())
        return WebAXObject();

    return WebAXObject(m_private->titleUIElement());
}

WebURL WebAXObject::url() const
{
    if (isDetached())
        return WebURL();

    return m_private->url();
}

bool WebAXObject::supportsRangeValue() const
{
    if (isDetached())
        return false;

    return m_private->supportsRangeValue();
}

WebString WebAXObject::valueDescription() const
{
    if (isDetached())
        return WebString();

    return m_private->valueDescription();
}

float WebAXObject::valueForRange() const
{
    if (isDetached())
        return 0.0;

    return m_private->valueForRange();
}

float WebAXObject::maxValueForRange() const
{
    if (isDetached())
        return 0.0;

    return m_private->maxValueForRange();
}

float WebAXObject::minValueForRange() const
{
    if (isDetached())
        return 0.0;

    return m_private->minValueForRange();
}

WebNode WebAXObject::node() const
{
    if (isDetached())
        return WebNode();

    Node* node = m_private->node();
    if (!node)
        return WebNode();

    return WebNode(node);
}

WebDocument WebAXObject::document() const
{
    if (isDetached())
        return WebDocument();

    Document* document = m_private->document();
    if (!document)
        return WebDocument();

    return WebDocument(document);
}

bool WebAXObject::hasComputedStyle() const
{
    if (isDetached())
        return false;

    Document* document = m_private->document();
    if (document)
        document->updateStyleIfNeeded();

    Node* node = m_private->node();
    if (!node)
        return false;

    return node->computedStyle();
}

WebString WebAXObject::computedStyleDisplay() const
{
    if (isDetached())
        return WebString();

    Document* document = m_private->document();
    if (document)
        document->updateStyleIfNeeded();

    Node* node = m_private->node();
    if (!node)
        return WebString();

    RenderStyle* renderStyle = node->computedStyle();
    if (!renderStyle)
        return WebString();

    return WebString(CSSPrimitiveValue::create(renderStyle->display())->getStringValue());
}

bool WebAXObject::accessibilityIsIgnored() const
{
    if (isDetached())
        return false;

    return m_private->accessibilityIsIgnored();
}

bool WebAXObject::lineBreaks(WebVector<int>& result) const
{
    if (isDetached())
        return false;

    Vector<int> lineBreaksVector;
    m_private->lineBreaks(lineBreaksVector);

    size_t vectorSize = lineBreaksVector.size();
    WebVector<int> lineBreaksWebVector(vectorSize);
    for (size_t i = 0; i< vectorSize; i++)
        lineBreaksWebVector[i] = lineBreaksVector[i];
    result.swap(lineBreaksWebVector);

    return true;
}

unsigned WebAXObject::columnCount() const
{
    if (isDetached())
        return false;

    if (!m_private->isAccessibilityTable())
        return 0;

    return static_cast<WebCore::AccessibilityTable*>(m_private.get())->columnCount();
}

unsigned WebAXObject::rowCount() const
{
    if (isDetached())
        return false;

    if (!m_private->isAccessibilityTable())
        return 0;

    return static_cast<WebCore::AccessibilityTable*>(m_private.get())->rowCount();
}

WebAXObject WebAXObject::cellForColumnAndRow(unsigned column, unsigned row) const
{
    if (isDetached())
        return WebAXObject();

    if (!m_private->isAccessibilityTable())
        return WebAXObject();

    WebCore::AccessibilityTableCell* cell = static_cast<WebCore::AccessibilityTable*>(m_private.get())->cellForColumnAndRow(column, row);
    return WebAXObject(static_cast<WebCore::AccessibilityObject*>(cell));
}

WebAXObject WebAXObject::headerContainerObject() const
{
    if (isDetached())
        return WebAXObject();

    if (!m_private->isAccessibilityTable())
        return WebAXObject();

    return WebAXObject(static_cast<WebCore::AccessibilityTable*>(m_private.get())->headerContainer());
}

WebAXObject WebAXObject::rowAtIndex(unsigned rowIndex) const
{
    if (isDetached())
        return WebAXObject();

    if (!m_private->isAccessibilityTable())
        return WebAXObject();

    const AccessibilityObject::AccessibilityChildrenVector& rows = static_cast<WebCore::AccessibilityTable*>(m_private.get())->rows();
    if (rowIndex < rows.size())
        return WebAXObject(rows[rowIndex]);

    return WebAXObject();
}

WebAXObject WebAXObject::columnAtIndex(unsigned columnIndex) const
{
    if (isDetached())
        return WebAXObject();

    if (!m_private->isAccessibilityTable())
        return WebAXObject();

    const AccessibilityObject::AccessibilityChildrenVector& columns = static_cast<WebCore::AccessibilityTable*>(m_private.get())->columns();
    if (columnIndex < columns.size())
        return WebAXObject(columns[columnIndex]);

    return WebAXObject();
}

unsigned WebAXObject::rowIndex() const
{
    if (isDetached())
        return 0;

    if (!m_private->isTableRow())
        return 0;

    return static_cast<WebCore::AccessibilityTableRow*>(m_private.get())->rowIndex();
}

WebAXObject WebAXObject::rowHeader() const
{
    if (isDetached())
        return WebAXObject();

    if (!m_private->isTableRow())
        return WebAXObject();

    return WebAXObject(static_cast<WebCore::AccessibilityTableRow*>(m_private.get())->headerObject());
}

unsigned WebAXObject::columnIndex() const
{
    if (isDetached())
        return 0;

    if (m_private->roleValue() != ColumnRole)
        return 0;

    return static_cast<WebCore::AccessibilityTableColumn*>(m_private.get())->columnIndex();
}

WebAXObject WebAXObject::columnHeader() const
{
    if (isDetached())
        return WebAXObject();

    if (m_private->roleValue() != ColumnRole)
        return WebAXObject();

    return WebAXObject(static_cast<WebCore::AccessibilityTableColumn*>(m_private.get())->headerObject());
}

unsigned WebAXObject::cellColumnIndex() const
{
    if (isDetached())
        return 0;

    if (!m_private->isTableCell())
        return 0;

    pair<unsigned, unsigned> columnRange;
    static_cast<WebCore::AccessibilityTableCell*>(m_private.get())->columnIndexRange(columnRange);
    return columnRange.first;
}

unsigned WebAXObject::cellColumnSpan() const
{
    if (isDetached())
        return 0;

    if (!m_private->isTableCell())
        return 0;

    pair<unsigned, unsigned> columnRange;
    static_cast<WebCore::AccessibilityTableCell*>(m_private.get())->columnIndexRange(columnRange);
    return columnRange.second;
}

unsigned WebAXObject::cellRowIndex() const
{
    if (isDetached())
        return 0;

    if (!m_private->isTableCell())
        return 0;

    pair<unsigned, unsigned> rowRange;
    static_cast<WebCore::AccessibilityTableCell*>(m_private.get())->rowIndexRange(rowRange);
    return rowRange.first;
}

unsigned WebAXObject::cellRowSpan() const
{
    if (isDetached())
        return 0;

    if (!m_private->isTableCell())
        return 0;

    pair<unsigned, unsigned> rowRange;
    static_cast<WebCore::AccessibilityTableCell*>(m_private.get())->rowIndexRange(rowRange);
    return rowRange.second;
}

void WebAXObject::scrollToMakeVisible() const
{
    if (!isDetached())
        m_private->scrollToMakeVisible();
}

void WebAXObject::scrollToMakeVisibleWithSubFocus(const WebRect& subfocus) const
{
    if (!isDetached())
        m_private->scrollToMakeVisibleWithSubFocus(subfocus);
}

void WebAXObject::scrollToGlobalPoint(const WebPoint& point) const
{
    if (!isDetached())
        m_private->scrollToGlobalPoint(point);
}

WebAXObject::WebAXObject(const WTF::PassRefPtr<WebCore::AccessibilityObject>& object)
    : m_private(object)
{
}

WebAXObject& WebAXObject::operator=(const WTF::PassRefPtr<WebCore::AccessibilityObject>& object)
{
    m_private = object;
    return *this;
}

WebAXObject::operator WTF::PassRefPtr<WebCore::AccessibilityObject>() const
{
    return m_private.get();
}

} // namespace WebKit
