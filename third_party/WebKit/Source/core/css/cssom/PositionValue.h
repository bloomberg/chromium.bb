// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PositionValue_h
#define PositionValue_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/css/cssom/LengthValue.h"
#include "core/css/cssom/StyleValue.h"

namespace blink {

class CORE_EXPORT PositionValue final : public StyleValue {
    WTF_MAKE_NONCOPYABLE(PositionValue);
    DEFINE_WRAPPERTYPEINFO();
public:
    static PositionValue* create(const LengthValue* x, const LengthValue* y)
    {
        return new PositionValue(x, y);
    }

    // Bindings require a non const return value.
    LengthValue* x() const { return const_cast<LengthValue*>(m_x.get()); }
    LengthValue* y() const { return const_cast<LengthValue*>(m_y.get()); }

    StyleValueType type() const override { return PositionValueType; }

    PassRefPtrWillBeRawPtr<CSSValue> toCSSValue() const override;

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_x);
        visitor->trace(m_y);
        StyleValue::trace(visitor);
    }

protected:
    PositionValue(const LengthValue* x, const LengthValue* y) : m_x(x),
        m_y(y) {}

    Member<const LengthValue> m_x;
    Member<const LengthValue> m_y;

};

} // namespace blink

#endif
