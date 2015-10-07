// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSCustomIdentValue_h
#define CSSCustomIdentValue_h

#include "core/css/CSSValue.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

namespace blink {

class CSSCustomIdentValue : public CSSValue {
public:
    static PassRefPtrWillBeRawPtr<CSSCustomIdentValue> create(const String& str)
    {
        return adoptRefWillBeNoop(new CSSCustomIdentValue(str));
    }

    String value() const { return m_string; }

    String customCSSText() const;

    bool equals(const CSSCustomIdentValue& other) const
    {
        return m_string == other.m_string;
    }

    DECLARE_TRACE_AFTER_DISPATCH();

private:
    CSSCustomIdentValue(const String&);

    String m_string;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSCustomIdentValue, isCustomIdentValue());

} // namespace blink

#endif // CSSCustomIdentValue_h
