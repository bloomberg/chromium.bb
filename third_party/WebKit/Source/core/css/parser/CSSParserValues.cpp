/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#include "core/css/parser/CSSParserValues.h"

#include "core/css/CSSFunctionValue.h"
#include "core/css/parser/CSSParserToken.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSPropertyParser.h"
#include "core/html/parser/HTMLParserIdioms.h"

namespace blink {

using namespace WTF;

CSSParserValueList::CSSParserValueList(CSSParserTokenRange range)
: m_current(0)
{
    Vector<CSSParserValueList*> stack;
    Vector<int> bracketCounts;
    stack.append(this);
    bracketCounts.append(0);
    while (!range.atEnd()) {
        ASSERT(stack.size() == bracketCounts.size());
        ASSERT(!stack.isEmpty());
        const CSSParserToken& token = range.peek();
        if (token.type() != FunctionToken)
            range.consume();
        CSSParserValue value;
        switch (token.type()) {
        case FunctionToken: {
            if (token.valueEqualsIgnoringCase("url")) {
                range.consume();
                const CSSParserToken& next = range.consumeIncludingWhitespace();
                if (next.type() == BadStringToken || range.consume().type() != RightParenthesisToken) {
                    destroyAndClear();
                    return;
                }
                ASSERT(next.type() == StringToken);
                value.id = CSSValueInvalid;
                value.isInt = false;
                value.m_unit = CSSParserValue::URI;
                value.string = next.value();
                break;
            } else if (token.valueEqualsIgnoringCase("var")) {
                destroyAndClear();
                return;
            }

            value.id = CSSValueInvalid;
            value.isInt = false;

            CSSValueID id = token.functionId();
            if (id == CSSValueCalc || id == CSSValueWebkitCalc) {
                value.m_unit = CSSParserValue::CalcFunction;
                value.calcFunction = new CSSParserCalcFunction(range.consumeBlock());
                break;
            }
            range.consume();
            value.m_unit = CSSParserValue::Function;
            CSSParserFunction* function = new CSSParserFunction;
            function->id = id;
            CSSParserValueList* list = new CSSParserValueList;
            function->args = adoptPtr(list);

            value.function = function;

            stack.last()->addValue(value);
            stack.append(list);
            bracketCounts.append(0);
            continue;
        }
        case LeftParenthesisToken: {
            CSSParserValueList* list = new CSSParserValueList;
            value.setFromValueList(adoptPtr(list));
            stack.last()->addValue(value);
            stack.append(list);
            bracketCounts.append(0);
            continue;
        }
        case RightParenthesisToken: {
            if (bracketCounts.last() == 0) {
                stack.removeLast();
                bracketCounts.removeLast();
                if (bracketCounts.isEmpty()) {
                    destroyAndClear();
                    return;
                }
                continue;
            }
            bracketCounts.last()--;
            value.setFromOperator(')');
            break;
        }
        case IdentToken: {
            value.id = token.id();
            value.isInt = false;
            value.m_unit = CSSParserValue::Identifier;
            value.string = token.value();
            break;
        }
        case DimensionToken:
            if (token.unitType() == CSSPrimitiveValue::UnitType::Unknown) {
                // Unknown dimensions are handled as a list of two values
                value.m_unit = CSSParserValue::DimensionList;
                CSSParserValueList* list = new CSSParserValueList;
                value.valueList = list;
                value.id = CSSValueInvalid;

                CSSParserValue number;
                number.setFromNumber(token.numericValue(), CSSPrimitiveValue::UnitType::Number);
                number.isInt = (token.numericValueType() == IntegerValueType);
                list->addValue(number);

                CSSParserValue unit;
                unit.string = token.value();
                unit.m_unit = CSSParserValue::Identifier;
                list->addValue(unit);

                break;
            }
            // fallthrough
        case NumberToken:
        case PercentageToken:
            value.setFromNumber(token.numericValue(), token.unitType());
            value.isInt = (token.numericValueType() == IntegerValueType);
            break;
        case UnicodeRangeToken: {
            value.id = CSSValueInvalid;
            value.isInt = false;
            value.m_unicodeRange.start = token.unicodeRangeStart();
            value.m_unicodeRange.end = token.unicodeRangeEnd();
            value.m_unit = CSSParserValue::UnicodeRange;
            break;
        }
        case HashToken:
        case StringToken:
        case UrlToken: {
            value.id = CSSValueInvalid;
            value.isInt = false;
            if (token.type() == HashToken)
                value.m_unit = CSSParserValue::HexColor;
            else if (token.type() == StringToken)
                value.m_unit = CSSParserValue::String;
            else
                value.m_unit = CSSParserValue::URI;
            value.string = token.value();
            break;
        }
        case DelimiterToken:
            value.setFromOperator(token.delimiter());
            break;
        case CommaToken:
            value.setFromOperator(',');
            break;
        case LeftBracketToken:
            value.setFromOperator('[');
            break;
        case RightBracketToken:
            value.setFromOperator(']');
            break;
        case LeftBraceToken:
            value.setFromOperator('{');
            break;
        case RightBraceToken:
            value.setFromOperator('}');
            break;
        case WhitespaceToken:
            continue;
        case CommentToken:
        case EOFToken:
            ASSERT_NOT_REACHED();
        case CDOToken:
        case CDCToken:
        case AtKeywordToken:
        case IncludeMatchToken:
        case DashMatchToken:
        case PrefixMatchToken:
        case SuffixMatchToken:
        case SubstringMatchToken:
        case ColumnToken:
        case ColonToken:
        case SemicolonToken:
            destroyAndClear();
            return;
        case BadStringToken:
        case BadUrlToken:
            destroyAndClear();
            return;
        }
        stack.last()->addValue(value);
    }

    CSSParserValue rightParenthesis;
    rightParenthesis.setFromOperator(')');
    while (!stack.isEmpty()) {
        while (bracketCounts.last() > 0) {
            bracketCounts.last()--;
            stack.last()->addValue(rightParenthesis);
        }
        stack.removeLast();
        bracketCounts.removeLast();
    }
}

static void destroy(Vector<CSSParserValue, 4>& values)
{
    size_t numValues = values.size();
    for (size_t i = 0; i < numValues; i++) {
        if (values[i].m_unit == CSSParserValue::Function)
            delete values[i].function;
        else if (values[i].m_unit == CSSParserValue::CalcFunction)
            delete values[i].calcFunction;
        else if (values[i].m_unit == CSSParserValue::ValueList
            || values[i].m_unit == CSSParserValue::DimensionList)
            delete values[i].valueList;
    }
}

void CSSParserValueList::destroyAndClear()
{
    destroy(m_values);
    clearAndLeakValues();
}

CSSParserValueList::~CSSParserValueList()
{
    destroy(m_values);
}

void CSSParserValueList::addValue(const CSSParserValue& v)
{
    m_values.append(v);
}

} // namespace blink
