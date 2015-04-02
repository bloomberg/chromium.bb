/*
 * Copyright (C) Research In Motion Limited 2010-2012. All rights reserved.
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

#include "config.h"
#include "core/layout/svg/SVGTextQuery.h"

#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/line/InlineFlowBox.h"
#include "core/layout/svg/LayoutSVGInlineText.h"
#include "core/layout/svg/SVGTextMetrics.h"
#include "core/layout/svg/line/SVGInlineTextBox.h"
#include "platform/FloatConversion.h"
#include "wtf/MathExtras.h"

namespace blink {

// Base structure for callback user data
struct SVGTextQuery::Data {
    Data()
        : isVerticalText(false)
        , processedCharacters(0)
        , textLayoutObject(0)
        , textBox(0)
    {
    }

    bool isVerticalText;
    unsigned processedCharacters;
    LayoutSVGInlineText* textLayoutObject;
    const SVGInlineTextBox* textBox;
};

static inline InlineFlowBox* flowBoxForLayoutObject(LayoutObject* layoutObject)
{
    if (!layoutObject)
        return 0;

    if (layoutObject->isLayoutBlock()) {
        // If we're given a block element, it has to be a LayoutSVGText.
        ASSERT(layoutObject->isSVGText());
        LayoutBlockFlow* layoutBlockFlow = toLayoutBlockFlow(layoutObject);

        // LayoutSVGText only ever contains a single line box.
        InlineFlowBox* flowBox = layoutBlockFlow->firstLineBox();
        ASSERT(flowBox == layoutBlockFlow->lastLineBox());
        return flowBox;
    }

    if (layoutObject->isLayoutInline()) {
        // We're given a LayoutSVGInline or objects that derive from it (LayoutSVGTSpan / LayoutSVGTextPath)
        LayoutInline* layoutInline = toLayoutInline(layoutObject);

        // LayoutSVGInline only ever contains a single line box.
        InlineFlowBox* flowBox = layoutInline->firstLineBox();
        ASSERT(flowBox == layoutInline->lastLineBox());
        return flowBox;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

SVGTextQuery::SVGTextQuery(LayoutObject* layoutObject)
{
    collectTextBoxesInFlowBox(flowBoxForLayoutObject(layoutObject));
}

void SVGTextQuery::collectTextBoxesInFlowBox(InlineFlowBox* flowBox)
{
    if (!flowBox)
        return;

    for (InlineBox* child = flowBox->firstChild(); child; child = child->nextOnLine()) {
        if (child->isInlineFlowBox()) {
            // Skip generated content.
            if (!child->layoutObject().node())
                continue;

            collectTextBoxesInFlowBox(toInlineFlowBox(child));
            continue;
        }

        if (child->isSVGInlineTextBox())
            m_textBoxes.append(toSVGInlineTextBox(child));
    }
}

bool SVGTextQuery::executeQuery(Data* queryData, ProcessTextFragmentCallback fragmentCallback) const
{
    unsigned processedCharacters = 0;
    unsigned textBoxCount = m_textBoxes.size();

    // Loop over all text boxes
    for (unsigned textBoxPosition = 0; textBoxPosition < textBoxCount; ++textBoxPosition) {
        queryData->textBox = m_textBoxes.at(textBoxPosition);
        queryData->textLayoutObject = &toLayoutSVGInlineText(queryData->textBox->layoutObject());
        ASSERT(queryData->textLayoutObject->style());

        queryData->isVerticalText = queryData->textLayoutObject->style()->svgStyle().isVerticalWritingMode();
        const Vector<SVGTextFragment>& fragments = queryData->textBox->textFragments();

        // Loop over all text fragments in this text box, firing a callback for each.
        unsigned fragmentCount = fragments.size();
        for (unsigned i = 0; i < fragmentCount; ++i) {
            const SVGTextFragment& fragment = fragments.at(i);
            if ((this->*fragmentCallback)(queryData, fragment))
                return true;

            processedCharacters += fragment.length;
        }

        queryData->processedCharacters = processedCharacters;
    }

    return false;
}

bool SVGTextQuery::mapStartEndPositionsIntoFragmentCoordinates(Data* queryData, const SVGTextFragment& fragment, int& startPosition, int& endPosition) const
{
    // Make <startPosition, endPosition> offsets relative to the current text box.
    startPosition -= queryData->processedCharacters;
    endPosition -= queryData->processedCharacters;

    // Reuse the same logic used for text selection & painting, to map our
    // query start/length into start/endPositions of the current text fragment.
    if (!queryData->textBox->mapStartEndPositionsIntoFragmentCoordinates(fragment, startPosition, endPosition))
        return false;

    modifyStartEndPositionsRespectingLigatures(queryData, fragment, startPosition, endPosition);
    ASSERT(startPosition < endPosition);
    return true;
}

void SVGTextQuery::modifyStartEndPositionsRespectingLigatures(Data* queryData, const SVGTextFragment& fragment, int& startPosition, int& endPosition) const
{
    const Vector<SVGTextMetrics>& textMetricsValues = queryData->textLayoutObject->layoutAttributes()->textMetricsValues();

    unsigned textMetricsOffset = fragment.metricsListOffset;
    int fragmentOffset = 0;
    int fragmentEnd = static_cast<int>(fragment.length);

    // Find the text metrics cell that start at or contain the character startPosition.
    while (fragmentOffset < fragmentEnd) {
        const SVGTextMetrics& metrics = textMetricsValues[textMetricsOffset];
        int glyphEnd = fragmentOffset + metrics.length();
        if (startPosition < glyphEnd)
            break;
        fragmentOffset = glyphEnd;
        textMetricsOffset++;
    }

    startPosition = fragmentOffset;

    // Find the text metrics cell that contain or ends at the character endPosition.
    while (fragmentOffset < fragmentEnd) {
        const SVGTextMetrics& metrics = textMetricsValues[textMetricsOffset];
        fragmentOffset += metrics.length();
        if (fragmentOffset >= endPosition)
            break;
        textMetricsOffset++;
    }

    endPosition = fragmentOffset;
}

// numberOfCharacters() implementation
bool SVGTextQuery::numberOfCharactersCallback(Data*, const SVGTextFragment&) const
{
    // no-op
    return false;
}

unsigned SVGTextQuery::numberOfCharacters() const
{
    Data data;
    executeQuery(&data, &SVGTextQuery::numberOfCharactersCallback);
    return data.processedCharacters;
}

// textLength() implementation
struct TextLengthData : SVGTextQuery::Data {
    TextLengthData()
        : textLength(0)
    {
    }

    float textLength;
};

bool SVGTextQuery::textLengthCallback(Data* queryData, const SVGTextFragment& fragment) const
{
    TextLengthData* data = static_cast<TextLengthData*>(queryData);
    data->textLength += queryData->isVerticalText ? fragment.height : fragment.width;
    return false;
}

float SVGTextQuery::textLength() const
{
    TextLengthData data;
    executeQuery(&data, &SVGTextQuery::textLengthCallback);
    return data.textLength;
}

// subStringLength() implementation
struct SubStringLengthData : SVGTextQuery::Data {
    SubStringLengthData(unsigned queryStartPosition, unsigned queryLength)
        : startPosition(queryStartPosition)
        , length(queryLength)
        , subStringLength(0)
    {
    }

    unsigned startPosition;
    unsigned length;

    float subStringLength;
};

bool SVGTextQuery::subStringLengthCallback(Data* queryData, const SVGTextFragment& fragment) const
{
    SubStringLengthData* data = static_cast<SubStringLengthData*>(queryData);

    int startPosition = data->startPosition;
    int endPosition = startPosition + data->length;
    if (!mapStartEndPositionsIntoFragmentCoordinates(queryData, fragment, startPosition, endPosition))
        return false;

    SVGTextMetrics metrics = SVGTextMetrics::measureCharacterRange(queryData->textLayoutObject, fragment.characterOffset + startPosition, endPosition - startPosition, queryData->textBox->direction());
    data->subStringLength += queryData->isVerticalText ? metrics.height() : metrics.width();
    return false;
}

float SVGTextQuery::subStringLength(unsigned startPosition, unsigned length) const
{
    SubStringLengthData data(startPosition, length);
    executeQuery(&data, &SVGTextQuery::subStringLengthCallback);
    return data.subStringLength;
}

// startPositionOfCharacter() implementation
struct StartPositionOfCharacterData : SVGTextQuery::Data {
    StartPositionOfCharacterData(unsigned queryPosition)
        : position(queryPosition)
    {
    }

    unsigned position;
    FloatPoint startPosition;
};

static FloatPoint calculateGlyphPositionWithoutTransform(const SVGTextQuery::Data* queryData, const SVGTextFragment& fragment, int offsetInFragment)
{
    float glyphOffsetInDirection = 0;
    if (offsetInFragment) {
        SVGTextMetrics metrics = SVGTextMetrics::measureCharacterRange(queryData->textLayoutObject, fragment.characterOffset, offsetInFragment, queryData->textBox->direction());
        if (queryData->isVerticalText)
            glyphOffsetInDirection = metrics.height();
        else
            glyphOffsetInDirection = metrics.width();
    }

    if (!queryData->textBox->isLeftToRightDirection()) {
        float fragmentExtent = queryData->isVerticalText ? fragment.height : fragment.width;
        glyphOffsetInDirection = fragmentExtent - glyphOffsetInDirection;
    }

    FloatPoint glyphPosition(fragment.x, fragment.y);
    if (queryData->isVerticalText)
        glyphPosition.move(0, glyphOffsetInDirection);
    else
        glyphPosition.move(glyphOffsetInDirection, 0);

    return glyphPosition;
}

static FloatPoint calculateGlyphPosition(const SVGTextQuery::Data* queryData, const SVGTextFragment& fragment, int offsetInFragment)
{
    FloatPoint glyphPosition = calculateGlyphPositionWithoutTransform(queryData, fragment, offsetInFragment);
    AffineTransform fragmentTransform;
    fragment.buildFragmentTransform(fragmentTransform, SVGTextFragment::TransformIgnoringTextLength);
    if (!fragmentTransform.isIdentity())
        glyphPosition = fragmentTransform.mapPoint(glyphPosition);

    return glyphPosition;
}

bool SVGTextQuery::startPositionOfCharacterCallback(Data* queryData, const SVGTextFragment& fragment) const
{
    StartPositionOfCharacterData* data = static_cast<StartPositionOfCharacterData*>(queryData);

    int startPosition = data->position;
    int endPosition = startPosition + 1;
    if (!mapStartEndPositionsIntoFragmentCoordinates(queryData, fragment, startPosition, endPosition))
        return false;

    data->startPosition = calculateGlyphPosition(queryData, fragment, startPosition);
    return true;
}

FloatPoint SVGTextQuery::startPositionOfCharacter(unsigned position) const
{
    StartPositionOfCharacterData data(position);
    executeQuery(&data, &SVGTextQuery::startPositionOfCharacterCallback);
    return data.startPosition;
}

// endPositionOfCharacter() implementation
struct EndPositionOfCharacterData : SVGTextQuery::Data {
    EndPositionOfCharacterData(unsigned queryPosition)
        : position(queryPosition)
    {
    }

    unsigned position;
    FloatPoint endPosition;
};

bool SVGTextQuery::endPositionOfCharacterCallback(Data* queryData, const SVGTextFragment& fragment) const
{
    EndPositionOfCharacterData* data = static_cast<EndPositionOfCharacterData*>(queryData);

    int startPosition = data->position;
    int endPosition = startPosition + 1;
    if (!mapStartEndPositionsIntoFragmentCoordinates(queryData, fragment, startPosition, endPosition))
        return false;

    // TODO(fs): mapStartEndPositionsIntoFragmentCoordinates(...) above applies
    // some heuristics for ligatures, so why not just use endPosition here?
    // (rather than startPosition+1)
    data->endPosition = calculateGlyphPosition(queryData, fragment, startPosition + 1);
    return true;
}

FloatPoint SVGTextQuery::endPositionOfCharacter(unsigned position) const
{
    EndPositionOfCharacterData data(position);
    executeQuery(&data, &SVGTextQuery::endPositionOfCharacterCallback);
    return data.endPosition;
}

// rotationOfCharacter() implementation
struct RotationOfCharacterData : SVGTextQuery::Data {
    RotationOfCharacterData(unsigned queryPosition)
        : position(queryPosition)
        , rotation(0)
    {
    }

    unsigned position;
    float rotation;
};

bool SVGTextQuery::rotationOfCharacterCallback(Data* queryData, const SVGTextFragment& fragment) const
{
    RotationOfCharacterData* data = static_cast<RotationOfCharacterData*>(queryData);

    int startPosition = data->position;
    int endPosition = startPosition + 1;
    if (!mapStartEndPositionsIntoFragmentCoordinates(queryData, fragment, startPosition, endPosition))
        return false;

    AffineTransform fragmentTransform;
    fragment.buildFragmentTransform(fragmentTransform, SVGTextFragment::TransformIgnoringTextLength);
    if (fragmentTransform.isIdentity()) {
        data->rotation = 0;
    } else {
        fragmentTransform.scale(1 / fragmentTransform.xScale(), 1 / fragmentTransform.yScale());
        data->rotation = narrowPrecisionToFloat(rad2deg(atan2(fragmentTransform.b(), fragmentTransform.a())));
    }

    return true;
}

float SVGTextQuery::rotationOfCharacter(unsigned position) const
{
    RotationOfCharacterData data(position);
    executeQuery(&data, &SVGTextQuery::rotationOfCharacterCallback);
    return data.rotation;
}

// extentOfCharacter() implementation
struct ExtentOfCharacterData : SVGTextQuery::Data {
    ExtentOfCharacterData(unsigned queryPosition)
        : position(queryPosition)
    {
    }

    unsigned position;
    FloatRect extent;
};

const SVGTextMetrics& findMetricsForCharacter(const Vector<SVGTextMetrics>& textMetricsValues, const SVGTextFragment& fragment, unsigned startInFragment)
{
    // Find the text metrics cell that start at or contain the character at |startInFragment|.
    unsigned textMetricsOffset = fragment.metricsListOffset;
    unsigned fragmentOffset = 0;
    while (fragmentOffset < fragment.length) {
        const SVGTextMetrics& metrics = textMetricsValues[textMetricsOffset++];
        unsigned glyphEnd = fragmentOffset + metrics.length();
        if (startInFragment < glyphEnd)
            break;
        fragmentOffset = glyphEnd;
    }
    return textMetricsValues[textMetricsOffset - 1];
}

static inline void calculateGlyphBoundaries(SVGTextQuery::Data* queryData, const SVGTextFragment& fragment, int startPosition, FloatRect& extent)
{
    float scalingFactor = queryData->textLayoutObject->scalingFactor();
    ASSERT(scalingFactor);

    FloatPoint glyphPosition = calculateGlyphPositionWithoutTransform(queryData, fragment, startPosition);
    glyphPosition.move(0, -queryData->textLayoutObject->scaledFont().fontMetrics().floatAscent() / scalingFactor);
    extent.setLocation(glyphPosition);

    // Use the SVGTextMetrics computed by SVGTextMetricsBuilder (which spends
    // time attempting to compute more correct glyph bounds already, handling
    // cursive scripts to some degree.)
    const Vector<SVGTextMetrics>& textMetricsValues = queryData->textLayoutObject->layoutAttributes()->textMetricsValues();
    const SVGTextMetrics& metrics = findMetricsForCharacter(textMetricsValues, fragment, startPosition);

    // TODO(fs): Negative glyph extents seems kind of weird to have, but
    // presently it can occur in some cases (like Arabic.)
    FloatSize glyphSize(std::max<float>(metrics.width(), 0), std::max<float>(metrics.height(), 0));
    extent.setSize(glyphSize);

    // If RTL, adjust the starting point to align with the LHS of the glyph bounding box.
    if (!queryData->textBox->isLeftToRightDirection()) {
        if (queryData->isVerticalText)
            extent.move(0, -glyphSize.height());
        else
            extent.move(-glyphSize.width(), 0);
    }

    AffineTransform fragmentTransform;
    fragment.buildFragmentTransform(fragmentTransform, SVGTextFragment::TransformIgnoringTextLength);

    extent = fragmentTransform.mapRect(extent);
}

static inline FloatRect calculateFragmentBoundaries(const LayoutSVGInlineText& textLayoutObject, const SVGTextFragment& fragment)
{
    float scalingFactor = textLayoutObject.scalingFactor();
    ASSERT(scalingFactor);
    float baseline = textLayoutObject.scaledFont().fontMetrics().floatAscent() / scalingFactor;

    AffineTransform fragmentTransform;
    FloatRect fragmentRect(fragment.x, fragment.y - baseline, fragment.width, fragment.height);
    fragment.buildFragmentTransform(fragmentTransform);
    return fragmentTransform.mapRect(fragmentRect);
}

bool SVGTextQuery::extentOfCharacterCallback(Data* queryData, const SVGTextFragment& fragment) const
{
    ExtentOfCharacterData* data = static_cast<ExtentOfCharacterData*>(queryData);

    int startPosition = data->position;
    int endPosition = startPosition + 1;
    if (!mapStartEndPositionsIntoFragmentCoordinates(queryData, fragment, startPosition, endPosition))
        return false;

    calculateGlyphBoundaries(queryData, fragment, startPosition, data->extent);
    return true;
}

FloatRect SVGTextQuery::extentOfCharacter(unsigned position) const
{
    ExtentOfCharacterData data(position);
    executeQuery(&data, &SVGTextQuery::extentOfCharacterCallback);
    return data.extent;
}

// characterNumberAtPosition() implementation
struct CharacterNumberAtPositionData : SVGTextQuery::Data {
    CharacterNumberAtPositionData(const FloatPoint& queryPosition)
        : position(queryPosition)
    {
    }

    FloatPoint position;
};

bool SVGTextQuery::characterNumberAtPositionCallback(Data* queryData, const SVGTextFragment& fragment) const
{
    CharacterNumberAtPositionData* data = static_cast<CharacterNumberAtPositionData*>(queryData);

    // Test the query point against the bounds of the entire fragment first.
    FloatRect fragmentExtents = calculateFragmentBoundaries(*queryData->textLayoutObject, fragment);
    if (!fragmentExtents.contains(data->position))
        return false;

    // Iterate through the glyphs in this fragment, and check if their extents
    // contain the query point.
    FloatRect extent;
    const Vector<SVGTextMetrics>& textMetrics = queryData->textLayoutObject->layoutAttributes()->textMetricsValues();
    unsigned textMetricsOffset = fragment.metricsListOffset;
    unsigned fragmentOffset = 0;
    while (fragmentOffset < fragment.length) {
        calculateGlyphBoundaries(queryData, fragment, fragmentOffset, extent);
        if (extent.contains(data->position)) {
            // Compute the character offset of the glyph within the text box
            // and add to processedCharacters.
            unsigned characterOffset = fragment.characterOffset + fragmentOffset;
            data->processedCharacters += characterOffset - data->textBox->start();
            return true;
        }
        fragmentOffset += textMetrics[textMetricsOffset].length();
        textMetricsOffset++;
    }
    return false;
}

int SVGTextQuery::characterNumberAtPosition(const FloatPoint& position) const
{
    CharacterNumberAtPositionData data(position);
    if (!executeQuery(&data, &SVGTextQuery::characterNumberAtPositionCallback))
        return -1;

    return data.processedCharacters;
}

}
