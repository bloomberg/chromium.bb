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

#include "config.h"
#include "core/css/parser/CSSParserValues.h"

#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSSelectorList.h"
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
    unsigned calcDepth = 0;
    while (!range.atEnd()) {
        ASSERT(stack.size() == bracketCounts.size());
        ASSERT(!stack.isEmpty());
        const CSSParserToken& token = range.consume();
        CSSParserValue value;
        switch (token.type()) {
        case FunctionToken: {
            if (equalIgnoringCase(token.value(), "url")) {
                const CSSParserToken& next = range.consumeIncludingWhitespaceAndComments();
                if (next.type() == BadStringToken || range.consume().type() != RightParenthesisToken) {
                    destroyAndClear();
                    return;
                }
                ASSERT(next.type() == StringToken);
                CSSParserString string;
                string.init(next.value());
                value.id = CSSValueInvalid;
                value.isInt = false;
                value.unit = CSSPrimitiveValue::CSS_URI;
                value.string = string;
                break;
            }

            CSSParserFunction* function = new CSSParserFunction;
            CSSParserString name;
            name.init(token.value());
            function->id = cssValueKeywordID(name);
            CSSParserValueList* list = new CSSParserValueList;
            function->args = adoptPtr(list);

            value.id = CSSValueInvalid;
            value.isInt = false;
            value.unit = CSSParserValue::Function;
            value.function = function;

            stack.last()->addValue(value);
            stack.append(list);
            bracketCounts.append(0);
            calcDepth += (function->id == CSSValueCalc || function->id == CSSValueWebkitCalc);
            continue;
        }
        case LeftParenthesisToken: {
            if (calcDepth == 0) {
                CSSParserValueList* list = new CSSParserValueList;
                value.setFromValueList(adoptPtr(list));
                stack.last()->addValue(value);
                stack.append(list);
                bracketCounts.append(0);
                continue;
            }
            bracketCounts.last()++;
            value.setFromOperator('(');
            break;
        }
        case RightParenthesisToken: {
            if (bracketCounts.last() == 0) {
                stack.removeLast();
                bracketCounts.removeLast();
                if (bracketCounts.isEmpty()) {
                    destroyAndClear();
                    return;
                }
                CSSParserValueList* currentList = stack.last();
                CSSParserValue* current = currentList->valueAt(currentList->size()-1);
                if (current->unit == CSSParserValue::Function) {
                    CSSValueID id = current->function->id;
                    calcDepth -= (id == CSSValueCalc || id == CSSValueWebkitCalc);
                }
                continue;
            }
            ASSERT(calcDepth > 0);
            bracketCounts.last()--;
            value.setFromOperator(')');
            break;
        }
        case IdentToken: {
            CSSParserString string;
            string.init(token.value());
            value.id = cssValueKeywordID(string);
            value.isInt = false;
            value.unit = CSSPrimitiveValue::CSS_IDENT;
            value.string = string;
            break;
        }
        case DimensionToken:
            if (!token.unitType()) {
                if (token.value() == "__qem") {
                    value.setFromNumber(token.numericValue(), CSSParserValue::Q_EMS);
                    value.isInt = (token.numericValueType() == IntegerValueType);
                    break;
                }

                // Unknown dimensions are handled as a list of two values
                value.unit = CSSParserValue::DimensionList;
                CSSParserValueList* list = new CSSParserValueList;
                value.valueList = list;

                CSSParserValue number;
                number.setFromNumber(token.numericValue());
                number.isInt = (token.numericValueType() == IntegerValueType);
                list->addValue(number);

                CSSParserString unitString;
                unitString.init(token.value());
                CSSParserValue unit;
                unit.string = unitString;
                unit.unit = CSSPrimitiveValue::CSS_IDENT;
                list->addValue(unit);

                break;
            }
            // fallthrough
        case NumberToken:
        case PercentageToken:
            value.setFromNumber(token.numericValue(), token.unitType());
            value.isInt = (token.numericValueType() == IntegerValueType);
            break;
        case HashToken:
            // FIXME: Move this logic to the property parser
            // This check prevents us from allowing #red and similar
            for (size_t i = 0; i < token.value().length(); ++i) {
                if (!isASCIIHexDigit(token.value()[i])) {
                    destroyAndClear();
                    return;
                }
            }
            // fallthrough
        case StringToken:
        case UnicodeRangeToken:
        case UrlToken: {
            CSSParserString string;
            string.init(token.value());
            value.id = CSSValueInvalid;
            value.isInt = false;
            if (token.type() == HashToken)
                value.unit = CSSParserValue::HexColor;
            else if (token.type() == StringToken)
                value.unit = CSSPrimitiveValue::CSS_STRING;
            else if (token.type() == UnicodeRangeToken)
                value.unit = CSSPrimitiveValue::CSS_UNICODE_RANGE;
            else
                value.unit = CSSPrimitiveValue::CSS_URI;
            value.string = string;
            break;
        }
        case DelimiterToken:
            value.setFromOperator(token.delimiter());
            if (calcDepth && token.delimiter() == '+' && (&token - 1)->type() != WhitespaceToken) {
                // calc(1px+ 2px) is invalid
                destroyAndClear();
                return;
            }
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
        case CommentToken:
            continue;
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
        case BadStringToken:
        case BadUrlToken:
        case ColonToken:
        case SemicolonToken:
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
        if (values[i].unit == CSSParserValue::Function)
            delete values[i].function;
        else if (values[i].unit == CSSParserValue::ValueList)
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

void CSSParserValueList::insertValueAt(unsigned i, const CSSParserValue& v)
{
    m_values.insert(i, v);
}

void CSSParserValueList::stealValues(CSSParserValueList& valueList)
{
    for (unsigned i = 0; i < valueList.size(); ++i)
        m_values.append(*(valueList.valueAt(i)));
    valueList.clearAndLeakValues();
}

CSSParserSelector::CSSParserSelector()
    : m_selector(adoptPtr(new CSSSelector()))
{
}

CSSParserSelector::CSSParserSelector(const QualifiedName& tagQName)
    : m_selector(adoptPtr(new CSSSelector(tagQName)))
{
}

CSSParserSelector::~CSSParserSelector()
{
    if (!m_tagHistory)
        return;
    Vector<OwnPtr<CSSParserSelector>, 16> toDelete;
    OwnPtr<CSSParserSelector> selector = m_tagHistory.release();
    while (true) {
        OwnPtr<CSSParserSelector> next = selector->m_tagHistory.release();
        toDelete.append(selector.release());
        if (!next)
            break;
        selector = next.release();
    }
}

void CSSParserSelector::adoptSelectorVector(Vector<OwnPtr<CSSParserSelector>>& selectorVector)
{
    CSSSelectorList* selectorList = new CSSSelectorList();
    selectorList->adoptSelectorVector(selectorVector);
    m_selector->setSelectorList(adoptPtr(selectorList));
}

void CSSParserSelector::setSelectorList(PassOwnPtr<CSSSelectorList> selectorList)
{
    m_selector->setSelectorList(selectorList);
}

bool CSSParserSelector::isSimple() const
{
    if (m_selector->selectorList() || m_selector->matchesPseudoElement())
        return false;

    if (!m_tagHistory)
        return true;

    if (m_selector->match() == CSSSelector::Tag) {
        // We can't check against anyQName() here because namespace may not be nullAtom.
        // Example:
        //     @namespace "http://www.w3.org/2000/svg";
        //     svg:not(:root) { ...
        if (m_selector->tagQName().localName() == starAtom)
            return m_tagHistory->isSimple();
    }

    return false;
}

void CSSParserSelector::insertTagHistory(CSSSelector::Relation before, PassOwnPtr<CSSParserSelector> selector, CSSSelector::Relation after)
{
    if (m_tagHistory)
        selector->setTagHistory(m_tagHistory.release());
    setRelation(before);
    selector->setRelation(after);
    m_tagHistory = selector;
}

void CSSParserSelector::appendTagHistory(CSSSelector::Relation relation, PassOwnPtr<CSSParserSelector> selector)
{
    CSSParserSelector* end = this;
    while (end->tagHistory())
        end = end->tagHistory();
    end->setRelation(relation);
    end->setTagHistory(selector);
}

void CSSParserSelector::prependTagSelector(const QualifiedName& tagQName, bool tagIsForNamespaceRule)
{
    OwnPtr<CSSParserSelector> second = adoptPtr(new CSSParserSelector);
    second->m_selector = m_selector.release();
    second->m_tagHistory = m_tagHistory.release();
    m_tagHistory = second.release();
    m_selector = adoptPtr(new CSSSelector(tagQName, tagIsForNamespaceRule));
}

bool CSSParserSelector::hasHostPseudoSelector() const
{
    for (CSSParserSelector* selector = const_cast<CSSParserSelector*>(this); selector; selector = selector->tagHistory()) {
        if (selector->pseudoType() == CSSSelector::PseudoHost || selector->pseudoType() == CSSSelector::PseudoHostContext)
            return true;
    }
    return false;
}

} // namespace blink
