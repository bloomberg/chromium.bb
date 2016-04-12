/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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

#include "core/css/parser/CSSPropertyParser.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/CSSCustomIdentValue.h"
#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSGridAutoRepeatValue.h"
#include "core/css/CSSGridLineNamesValue.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSValuePair.h"
#include "core/css/CSSValuePool.h"
#include "core/css/parser/CSSParserValues.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/style/GridArea.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

using namespace CSSPropertyParserHelpers;

void CSSPropertyParser::addProperty(CSSPropertyID propId, CSSValue* value, bool important, bool implicit)
{
    ASSERT(!isPropertyAlias(propId));

    int shorthandIndex = 0;
    bool setFromShorthand = false;

    if (m_currentShorthand) {
        Vector<StylePropertyShorthand, 4> shorthands;
        getMatchingShorthandsForLonghand(propId, &shorthands);
        setFromShorthand = true;
        if (shorthands.size() > 1)
            shorthandIndex = indexOfShorthandForLonghand(m_currentShorthand, shorthands);
    }

    m_parsedProperties->append(CSSProperty(propId, value, important, setFromShorthand, shorthandIndex, implicit));
}

void CSSPropertyParser::addExpandedPropertyForValue(CSSPropertyID propId, CSSValue* value, bool important)
{
    const StylePropertyShorthand& shorthand = shorthandForProperty(propId);
    unsigned shorthandLength = shorthand.length();
    if (!shorthandLength) {
        addProperty(propId, value, important);
        return;
    }

    ShorthandScope scope(this, propId);
    const CSSPropertyID* longhands = shorthand.properties();
    for (unsigned i = 0; i < shorthandLength; ++i)
        addProperty(longhands[i], value, important);
}

bool CSSPropertyParser::legacyParseShorthand(CSSPropertyID propertyID, bool important)
{
    switch (propertyID) {
    case CSSPropertyGrid:
        ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
        return parseGridShorthand(important);

    // The remaining shorthands are handled in CSSPropertyParser.cpp
    default:
        return false;
    }
}

bool CSSPropertyParser::parseGridShorthand(bool important)
{
    ShorthandScope scope(this, CSSPropertyGrid);
    ASSERT(shorthandForProperty(CSSPropertyGrid).length() == 8);

    CSSParserTokenRange rangeCopy = m_range;

    // 1- <grid-template>
    if (consumeGridTemplateShorthand(important)) {
        // It can only be specified the explicit or the implicit grid properties in a single grid declaration.
        // The sub-properties not specified are set to their initial value, as normal for shorthands.
        addProperty(CSSPropertyGridAutoFlow, cssValuePool().createImplicitInitialValue(), important);
        addProperty(CSSPropertyGridAutoColumns, cssValuePool().createImplicitInitialValue(), important);
        addProperty(CSSPropertyGridAutoRows, cssValuePool().createImplicitInitialValue(), important);
        addProperty(CSSPropertyGridColumnGap, cssValuePool().createImplicitInitialValue(), important);
        addProperty(CSSPropertyGridRowGap, cssValuePool().createImplicitInitialValue(), important);
        return true;
    }

    m_range = rangeCopy;

    // 2- <grid-auto-flow> [ <grid-auto-rows> [ / <grid-auto-columns> ]? ]
    CSSValueList* gridAutoFlow = consumeGridAutoFlow(m_range);
    if (!gridAutoFlow)
        return false;

    CSSValue* autoColumnsValue = nullptr;
    CSSValue* autoRowsValue = nullptr;

    if (!m_range.atEnd()) {
        autoRowsValue = consumeGridTrackSize(m_range, m_context.mode());
        if (!autoRowsValue)
            return false;
        if (consumeSlashIncludingWhitespace(m_range)) {
            autoColumnsValue = consumeGridTrackSize(m_range, m_context.mode());
            if (!autoColumnsValue)
                return false;
        }
        if (!m_range.atEnd())
            return false;
    } else {
        // Other omitted values are set to their initial values.
        autoColumnsValue = cssValuePool().createImplicitInitialValue();
        autoRowsValue = cssValuePool().createImplicitInitialValue();
    }

    // if <grid-auto-columns> value is omitted, it is set to the value specified for grid-auto-rows.
    if (!autoColumnsValue)
        autoColumnsValue = autoRowsValue;

    addProperty(CSSPropertyGridAutoFlow, gridAutoFlow, important);
    addProperty(CSSPropertyGridAutoColumns, autoColumnsValue, important);
    addProperty(CSSPropertyGridAutoRows, autoRowsValue, important);

    // It can only be specified the explicit or the implicit grid properties in a single grid declaration.
    // The sub-properties not specified are set to their initial value, as normal for shorthands.
    addProperty(CSSPropertyGridTemplateColumns, cssValuePool().createImplicitInitialValue(), important);
    addProperty(CSSPropertyGridTemplateRows, cssValuePool().createImplicitInitialValue(), important);
    addProperty(CSSPropertyGridTemplateAreas, cssValuePool().createImplicitInitialValue(), important);
    addProperty(CSSPropertyGridColumnGap, cssValuePool().createImplicitInitialValue(), important);
    addProperty(CSSPropertyGridRowGap, cssValuePool().createImplicitInitialValue(), important);

    return true;
}

bool allTracksAreFixedSized(CSSValueList& valueList)
{
    for (auto value : valueList) {
        if (value->isGridLineNamesValue())
            continue;
        // The auto-repeat value holds a <fixed-size> = <fixed-breadth> | minmax( <fixed-breadth>, <track-breadth> )
        if (value->isGridAutoRepeatValue()) {
            if (!allTracksAreFixedSized(toCSSValueList(*value)))
                return false;
            continue;
        }
        ASSERT(value->isPrimitiveValue() || (value->isFunctionValue() && toCSSFunctionValue(*value).item(0)));
        const CSSPrimitiveValue& primitiveValue = value->isPrimitiveValue()
            ? toCSSPrimitiveValue(*value)
            : toCSSPrimitiveValue(*toCSSFunctionValue(*value).item(0));
        CSSValueID valueID = primitiveValue.getValueID();
        if (valueID == CSSValueMinContent || valueID == CSSValueMaxContent || valueID == CSSValueAuto || primitiveValue.isFlex())
            return false;
    }
    return true;
}

static Vector<String> parseGridTemplateAreasColumnNames(const String& gridRowNames)
{
    ASSERT(!gridRowNames.isEmpty());
    Vector<String> columnNames;
    // Using StringImpl to avoid checks and indirection in every call to String::operator[].
    StringImpl& text = *gridRowNames.impl();

    StringBuilder areaName;
    for (unsigned i = 0; i < text.length(); ++i) {
        if (text[i] == ' ') {
            if (!areaName.isEmpty()) {
                columnNames.append(areaName.toString());
                areaName.clear();
            }
            continue;
        }
        if (text[i] == '.') {
            if (areaName == ".")
                continue;
            if (!areaName.isEmpty()) {
                columnNames.append(areaName.toString());
                areaName.clear();
            }
        } else {
            if (areaName == ".") {
                columnNames.append(areaName.toString());
                areaName.clear();
            }
        }

        areaName.append(text[i]);
    }

    if (!areaName.isEmpty())
        columnNames.append(areaName.toString());

    return columnNames;
}

bool parseGridTemplateAreasRow(const String& gridRowNames, NamedGridAreaMap& gridAreaMap, const size_t rowCount, size_t& columnCount)
{
    if (gridRowNames.isEmpty() || gridRowNames.containsOnlyWhitespace())
        return false;

    Vector<String> columnNames = parseGridTemplateAreasColumnNames(gridRowNames);
    if (!columnCount) {
        columnCount = columnNames.size();
        ASSERT(columnCount);
    } else if (columnCount != columnNames.size()) {
        // The declaration is invalid is all the rows don't have the number of columns.
        return false;
    }

    for (size_t currentCol = 0; currentCol < columnCount; ++currentCol) {
        const String& gridAreaName = columnNames[currentCol];

        // Unamed areas are always valid (we consider them to be 1x1).
        if (gridAreaName == ".")
            continue;

        // We handle several grid areas with the same name at once to simplify the validation code.
        size_t lookAheadCol;
        for (lookAheadCol = currentCol + 1; lookAheadCol < columnCount; ++lookAheadCol) {
            if (columnNames[lookAheadCol] != gridAreaName)
                break;
        }

        NamedGridAreaMap::iterator gridAreaIt = gridAreaMap.find(gridAreaName);
        if (gridAreaIt == gridAreaMap.end()) {
            gridAreaMap.add(gridAreaName, GridArea(GridSpan::translatedDefiniteGridSpan(rowCount, rowCount + 1), GridSpan::translatedDefiniteGridSpan(currentCol, lookAheadCol)));
        } else {
            GridArea& gridArea = gridAreaIt->value;

            // The following checks test that the grid area is a single filled-in rectangle.
            // 1. The new row is adjacent to the previously parsed row.
            if (rowCount != gridArea.rows.endLine())
                return false;

            // 2. The new area starts at the same position as the previously parsed area.
            if (currentCol != gridArea.columns.startLine())
                return false;

            // 3. The new area ends at the same position as the previously parsed area.
            if (lookAheadCol != gridArea.columns.endLine())
                return false;

            gridArea.rows = GridSpan::translatedDefiniteGridSpan(gridArea.rows.startLine(), gridArea.rows.endLine() + 1);
        }
        currentCol = lookAheadCol - 1;
    }

    return true;
}

} // namespace blink
