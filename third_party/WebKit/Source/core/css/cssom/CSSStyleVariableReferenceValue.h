// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSStyleVariableReferenceValue_h
#define CSSStyleVariableReferenceValue_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"

namespace blink {

class CORE_EXPORT CSSStyleVariableReferenceValue final : public GarbageCollectedFinalized<CSSStyleVariableReferenceValue>, public ScriptWrappable {
    WTF_MAKE_NONCOPYABLE(CSSStyleVariableReferenceValue);
    DEFINE_WRAPPERTYPEINFO();
public:
    virtual ~CSSStyleVariableReferenceValue() { }

    // TODO(anthonyhkf): add fallback: create(variable, fallback)
    static CSSStyleVariableReferenceValue* create(const String& variable)
    {
        return new CSSStyleVariableReferenceValue(variable);
    }

    DEFINE_INLINE_TRACE() { }

    const String& variable() const { return m_variable; }

protected:
    CSSStyleVariableReferenceValue(String variable): m_variable(variable) { }

    String m_variable;
};

} // namespace blink

#endif // CSSStyleVariableReference_h
