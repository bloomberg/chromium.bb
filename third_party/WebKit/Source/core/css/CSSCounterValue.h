/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
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

#ifndef CSSCounterValue_h
#define CSSCounterValue_h

#include "core/css/CSSCustomIdentValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValue.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CSSCounterValue : public CSSValue {
public:
    static PassRefPtrWillBeRawPtr<CSSCounterValue> create(PassRefPtrWillBeRawPtr<CSSCustomIdentValue> identifier, PassRefPtrWillBeRawPtr<CSSPrimitiveValue> listStyle, PassRefPtrWillBeRawPtr<CSSCustomIdentValue> separator)
    {
        return adoptRefWillBeNoop(new CSSCounterValue(identifier, listStyle, separator));
    }

    String identifier() const { return m_identifier->value(); }
    CSSValueID listStyle() const { return m_listStyle->getValueID(); }
    String separator() const { return m_separator->value(); }

    bool equals(const CSSCounterValue& other) const
    {
        return identifier() == other.identifier()
            && listStyle() == other.listStyle()
            && separator() == other.separator();
    }

    String customCSSText() const;

    DECLARE_TRACE_AFTER_DISPATCH();

private:
    CSSCounterValue(PassRefPtrWillBeRawPtr<CSSCustomIdentValue> identifier, PassRefPtrWillBeRawPtr<CSSPrimitiveValue> listStyle, PassRefPtrWillBeRawPtr<CSSCustomIdentValue> separator)
        : CSSValue(CounterClass)
        , m_identifier(identifier)
        , m_listStyle(listStyle)
        , m_separator(separator)
    {
        ASSERT(m_listStyle->isValueID());
    }

    RefPtrWillBeMember<CSSCustomIdentValue> m_identifier; // string
    RefPtrWillBeMember<CSSPrimitiveValue> m_listStyle; // ident
    RefPtrWillBeMember<CSSCustomIdentValue> m_separator; // string
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSCounterValue, isCounterValue());

} // namespace blink

#endif // CSSCounterValue_h
