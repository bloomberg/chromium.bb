// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PropertyRegistry_h
#define PropertyRegistry_h

#include "core/css/CSSSyntaxDescriptor.h"
#include "core/css/CSSValue.h"
#include "core/css/CSSVariableData.h"
#include "wtf/HashMap.h"
#include "wtf/RefPtr.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/AtomicStringHash.h"

namespace blink {

class PropertyRegistry : public GarbageCollected<PropertyRegistry> {
public:
    static PropertyRegistry* create()
    {
        return new PropertyRegistry();
    }

    class Registration : public GarbageCollectedFinalized<Registration> {
    public:
        Registration(const CSSSyntaxDescriptor& syntax, bool inherits, const CSSValue* initial)
        : m_syntax(syntax), m_inherits(inherits), m_initial(initial) { }

        const CSSSyntaxDescriptor& syntax() const { return m_syntax; }
        bool inherits() const { return m_inherits; }
        const CSSValue* initial() const { return m_initial; }

        DEFINE_INLINE_TRACE() { visitor->trace(m_initial); }

    private:
        const CSSSyntaxDescriptor m_syntax;
        const bool m_inherits;
        const Member<const CSSValue> m_initial;
    };

    void registerProperty(const AtomicString&, const CSSSyntaxDescriptor&, bool inherits, const CSSValue* initial);
    void unregisterProperty(const AtomicString&);
    const Registration* registration(const AtomicString&) const;

    DEFINE_INLINE_TRACE() { visitor->trace(m_registrations); }

private:
    HeapHashMap<AtomicString, Member<Registration>> m_registrations;
};

} // namespace blink

#endif // PropertyRegistry_h
