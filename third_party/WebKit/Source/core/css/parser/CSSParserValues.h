/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef CSSParserValues_h
#define CSSParserValues_h

#include "core/CSSValueKeywords.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParserString.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "wtf/Allocator.h"

namespace blink {

class QualifiedName;

struct CSSParserFunction;
struct CSSParserCalcFunction;
class CSSParserValueList;

struct CSSParserValue {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    CSSValueID id;
    bool isInt;
    union {
        double fValue;
        int iValue;
        CSSParserString string;
        CSSParserFunction* function;
        CSSParserCalcFunction* calcFunction;
        CSSParserValueList* valueList;
        struct {
            UChar32 start;
            UChar32 end;
        } m_unicodeRange;
    };
    enum {
        Operator  = 0x100000,
        Function  = 0x100001,
        CalcFunction  = 0x100002,
        ValueList = 0x100003,
        HexColor = 0x100004,
        Identifier = 0x100005,
        // Represents a dimension by a list of two values, a UnitType::Number and an Identifier
        DimensionList = 0x100006,
        // Represents a unicode range by a pair of UChar32 values
        UnicodeRange = 0x100007,
        String = 0x100008,
        URI = 0x100009,
    };
    int m_unit;
    CSSPrimitiveValue::UnitType unit() const { return static_cast<CSSPrimitiveValue::UnitType>(m_unit); }
    void setUnit(CSSPrimitiveValue::UnitType unit) { m_unit = static_cast<int>(unit); }

    inline void setFromNumber(double value, CSSPrimitiveValue::UnitType);
    inline void setFromOperator(UChar);
    inline void setFromValueList(PassOwnPtr<CSSParserValueList>);
};

class CORE_EXPORT CSSParserValueList {
    USING_FAST_MALLOC(CSSParserValueList);
public:
    CSSParserValueList()
        : m_current(0)
    {
    }
    CSSParserValueList(CSSParserTokenRange);
    ~CSSParserValueList();

    void addValue(const CSSParserValue&);

    unsigned size() const { return m_values.size(); }
    unsigned currentIndex() { return m_current; }
    CSSParserValue* current() { return m_current < m_values.size() ? &m_values[m_current] : nullptr; }
    CSSParserValue* next() { ++m_current; return current(); }
    CSSParserValue* previous()
    {
        if (!m_current)
            return 0;
        --m_current;
        return current();
    }
    void setCurrentIndex(unsigned index)
    {
        ASSERT(index < m_values.size());
        if (index < m_values.size())
            m_current = index;
    }

    CSSParserValue* valueAt(unsigned i) { return i < m_values.size() ? &m_values[i] : nullptr; }

    void clearAndLeakValues() { m_values.clear(); m_current = 0;}
    void destroyAndClear();

private:
    unsigned m_current;
    Vector<CSSParserValue, 4> m_values;
};

struct CSSParserFunction {
    USING_FAST_MALLOC(CSSParserFunction);
public:
    CSSValueID id;
    OwnPtr<CSSParserValueList> args;
};

struct CSSParserCalcFunction {
    USING_FAST_MALLOC(CSSParserCalcFunction);
public:
    CSSParserCalcFunction(CSSParserTokenRange args_) : args(args_) {}
    CSSParserTokenRange args;
};

inline void CSSParserValue::setFromNumber(double value, CSSPrimitiveValue::UnitType unit)
{
    id = CSSValueInvalid;
    isInt = false;
    fValue = value;
    this->setUnit(std::isfinite(value) ? unit : CSSPrimitiveValue::UnitType::Unknown);
}

inline void CSSParserValue::setFromOperator(UChar c)
{
    id = CSSValueInvalid;
    m_unit = Operator;
    iValue = c;
    isInt = false;
}

inline void CSSParserValue::setFromValueList(PassOwnPtr<CSSParserValueList> valueList)
{
    id = CSSValueInvalid;
    this->valueList = valueList.leakPtr();
    m_unit = ValueList;
    isInt = false;
}

} // namespace blink

#endif
