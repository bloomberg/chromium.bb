/*
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
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
 */

#include "core/layout/svg/SVGTextLayoutAttributesBuilder.h"

#include "core/layout/svg/LayoutSVGInline.h"
#include "core/layout/svg/LayoutSVGInlineText.h"
#include "core/layout/svg/LayoutSVGText.h"
#include "core/svg/SVGTextPositioningElement.h"

namespace blink {

namespace {

void updateLayoutAttributes(LayoutSVGInlineText& text, unsigned& valueListPosition, const SVGCharacterDataMap& allCharactersMap)
{
    SVGTextLayoutAttributes& attributes = *text.layoutAttributes();
    attributes.clear();

    const Vector<SVGTextMetrics>& metricsList = text.metricsList();
    auto metricsEnd = metricsList.end();
    unsigned currentPosition = 0;
    for (auto metrics = metricsList.begin(); metrics != metricsEnd; currentPosition += metrics->length(), ++metrics) {
        if (metrics->isEmpty())
            continue;

        auto it = allCharactersMap.find(valueListPosition + 1);
        if (it != allCharactersMap.end())
            attributes.characterDataMap().set(currentPosition + 1, it->value);

        // Increase the position in the value/attribute list with one for each
        // "character unit" (that will be displayed.)
        valueListPosition++;
    }
}

} // namespace

SVGTextLayoutAttributesBuilder::SVGTextLayoutAttributesBuilder()
    : m_characterCount(0)
{
}

void SVGTextLayoutAttributesBuilder::buildLayoutAttributes(LayoutSVGText& textRoot) const
{
    unsigned valueListPosition = 0;
    LayoutObject* child = textRoot.firstChild();
    while (child) {
        if (child->isSVGInlineText()) {
            updateLayoutAttributes(toLayoutSVGInlineText(*child), valueListPosition, m_characterDataMap);
        } else if (child->isSVGInline()) {
            // Visit children of text content elements.
            if (LayoutObject* inlineChild = toLayoutSVGInline(child)->firstChild()) {
                child = inlineChild;
                continue;
            }
        }
        child = child->nextInPreOrderAfterChildren(&textRoot);
    }
}

void SVGTextLayoutAttributesBuilder::buildLayoutAttributesForTextRoot(LayoutSVGText& textRoot)
{
    m_characterDataMap.clear();

    if (m_textPositions.isEmpty()) {
        m_characterCount = 0;
        collectTextPositioningElements(textRoot);
    }

    if (!m_characterCount)
        return;

    buildCharacterDataMap(textRoot);
    buildLayoutAttributes(textRoot);
}

static inline unsigned countCharactersInTextNode(const LayoutSVGInlineText& text)
{
    unsigned numCharacters = 0;
    for (const SVGTextMetrics& metrics : text.metricsList()) {
        if (metrics.isEmpty())
            continue;
        numCharacters++;
    }
    return numCharacters;
}

static SVGTextPositioningElement* positioningElementFromLayoutObject(LayoutObject& layoutObject)
{
    ASSERT(layoutObject.isSVGText() || layoutObject.isSVGInline());

    Node* node = layoutObject.node();
    ASSERT(node);
    ASSERT(node->isSVGElement());

    return isSVGTextPositioningElement(*node) ? toSVGTextPositioningElement(node) : nullptr;
}

void SVGTextLayoutAttributesBuilder::collectTextPositioningElements(LayoutBoxModelObject& start)
{
    ASSERT(!start.isSVGText() || m_textPositions.isEmpty());

    for (LayoutObject* child = start.slowFirstChild(); child; child = child->nextSibling()) {
        if (child->isSVGInlineText()) {
            m_characterCount += countCharactersInTextNode(toLayoutSVGInlineText(*child));
            continue;
        }

        if (!child->isSVGInline())
            continue;

        LayoutSVGInline& inlineChild = toLayoutSVGInline(*child);
        SVGTextPositioningElement* element = positioningElementFromLayoutObject(inlineChild);
        unsigned atPosition = m_textPositions.size();
        if (element)
            m_textPositions.append(TextPosition(element, m_characterCount));

        collectTextPositioningElements(inlineChild);

        if (!element)
            continue;

        // Update text position, after we're back from recursion.
        TextPosition& position = m_textPositions[atPosition];
        ASSERT(!position.length);
        position.length = m_characterCount - position.start;
    }
}

void SVGTextLayoutAttributesBuilder::buildCharacterDataMap(LayoutSVGText& textRoot)
{
    SVGTextPositioningElement* outermostTextElement = positioningElementFromLayoutObject(textRoot);
    ASSERT(outermostTextElement);

    // Grab outermost <text> element value lists and insert them in the character data map.
    TextPosition wholeTextPosition(outermostTextElement, 0, m_characterCount);
    fillCharacterDataMap(wholeTextPosition);

    // Fill character data map using child text positioning elements in top-down order.
    unsigned size = m_textPositions.size();
    for (unsigned i = 0; i < size; ++i)
        fillCharacterDataMap(m_textPositions[i]);

    // Handle x/y default attributes.
    SVGCharacterData& data = m_characterDataMap.add(1, SVGCharacterData()).storedValue->value;
    if (SVGTextLayoutAttributes::isEmptyValue(data.x))
        data.x = 0;
    if (SVGTextLayoutAttributes::isEmptyValue(data.y))
        data.y = 0;
}

namespace {

class AttributeListsIterator {
    STACK_ALLOCATED();
public:
    AttributeListsIterator(SVGTextPositioningElement*);

    bool hasAttributes() const
    {
        return m_xListRemaining || m_yListRemaining
            || m_dxListRemaining || m_dyListRemaining
            || m_rotateListRemaining;
    }
    void updateCharacterData(size_t index, SVGCharacterData&);

private:
    SVGLengthContext m_lengthContext;
    Member<SVGLengthList> m_xList;
    unsigned m_xListRemaining;
    Member<SVGLengthList> m_yList;
    unsigned m_yListRemaining;
    Member<SVGLengthList> m_dxList;
    unsigned m_dxListRemaining;
    Member<SVGLengthList> m_dyList;
    unsigned m_dyListRemaining;
    Member<SVGNumberList> m_rotateList;
    unsigned m_rotateListRemaining;
};

AttributeListsIterator::AttributeListsIterator(SVGTextPositioningElement* element)
    : m_lengthContext(element)
    , m_xList(element->x()->currentValue())
    , m_xListRemaining(m_xList->length())
    , m_yList(element->y()->currentValue())
    , m_yListRemaining(m_yList->length())
    , m_dxList(element->dx()->currentValue())
    , m_dxListRemaining(m_dxList->length())
    , m_dyList(element->dy()->currentValue())
    , m_dyListRemaining(m_dyList->length())
    , m_rotateList(element->rotate()->currentValue())
    , m_rotateListRemaining(m_rotateList->length())
{
}

inline void AttributeListsIterator::updateCharacterData(size_t index, SVGCharacterData& data)
{
    if (m_xListRemaining) {
        data.x = m_xList->at(index)->value(m_lengthContext);
        --m_xListRemaining;
    }
    if (m_yListRemaining) {
        data.y = m_yList->at(index)->value(m_lengthContext);
        --m_yListRemaining;
    }
    if (m_dxListRemaining) {
        data.dx = m_dxList->at(index)->value(m_lengthContext);
        --m_dxListRemaining;
    }
    if (m_dyListRemaining) {
        data.dy = m_dyList->at(index)->value(m_lengthContext);
        --m_dyListRemaining;
    }
    if (m_rotateListRemaining) {
        data.rotate = m_rotateList->at(std::min(index, m_rotateList->length() - 1))->value();
        // The last rotation value spans the whole scope.
        if (m_rotateListRemaining > 1)
            --m_rotateListRemaining;
    }
}

} // namespace

void SVGTextLayoutAttributesBuilder::fillCharacterDataMap(const TextPosition& position)
{
    AttributeListsIterator attrLists(position.element);
    for (unsigned i = 0; attrLists.hasAttributes() && i < position.length; ++i) {
        SVGCharacterData& data = m_characterDataMap.add(position.start + i + 1, SVGCharacterData()).storedValue->value;
        attrLists.updateCharacterData(i, data);
    }
}

DEFINE_TRACE(SVGTextLayoutAttributesBuilder::TextPosition)
{
    visitor->trace(element);
}

} // namespace blink
