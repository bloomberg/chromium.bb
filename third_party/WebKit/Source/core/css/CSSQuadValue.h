/*
 * Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef CSSQuadValue_h
#define CSSQuadValue_h

#include "core/CoreExport.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValue.h"
#include "wtf/RefPtr.h"

namespace blink {

class CORE_EXPORT CSSQuadValue : public CSSValue {
public:
    enum SerializationType {
        SerializeAsRect,
        SerializeAsQuad
    };

    static PassRefPtrWillBeRawPtr<CSSQuadValue> create(PassRefPtrWillBeRawPtr<CSSPrimitiveValue> top, PassRefPtrWillBeRawPtr<CSSPrimitiveValue> right, PassRefPtrWillBeRawPtr<CSSPrimitiveValue> bottom, PassRefPtrWillBeRawPtr<CSSPrimitiveValue> left, SerializationType serializationType)
    {
        return adoptRefWillBeNoop(new CSSQuadValue(top, right, bottom, left, serializationType));
    }

    CSSPrimitiveValue* top() const { return m_top.get(); }
    CSSPrimitiveValue* right() const { return m_right.get(); }
    CSSPrimitiveValue* bottom() const { return m_bottom.get(); }
    CSSPrimitiveValue* left() const { return m_left.get(); }

    SerializationType serializationType() { return m_serializationType; }

    String customCSSText() const;

    bool equals(const CSSQuadValue& other) const
    {
        return compareCSSValuePtr(m_top, other.m_top)
            && compareCSSValuePtr(m_right, other.m_right)
            && compareCSSValuePtr(m_left, other.m_left)
            && compareCSSValuePtr(m_bottom, other.m_bottom);
    }

    DECLARE_TRACE_AFTER_DISPATCH();

protected:
    CSSQuadValue(PassRefPtrWillBeRawPtr<CSSPrimitiveValue> top, PassRefPtrWillBeRawPtr<CSSPrimitiveValue> right, PassRefPtrWillBeRawPtr<CSSPrimitiveValue> bottom, PassRefPtrWillBeRawPtr<CSSPrimitiveValue> left, SerializationType serializationType)
        : CSSValue(QuadClass)
        , m_serializationType(serializationType)
        , m_top(top)
        , m_right(right)
        , m_bottom(bottom)
        , m_left(left) { }

private:
    SerializationType m_serializationType;
    RefPtrWillBeMember<CSSPrimitiveValue> m_top;
    RefPtrWillBeMember<CSSPrimitiveValue> m_right;
    RefPtrWillBeMember<CSSPrimitiveValue> m_bottom;
    RefPtrWillBeMember<CSSPrimitiveValue> m_left;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSQuadValue, isQuadValue());

} // namespace blink

#endif // CSSQuadValue_h
