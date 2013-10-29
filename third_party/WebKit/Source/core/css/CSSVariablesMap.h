/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CSSVariablesMap_h
#define CSSVariablesMap_h

#include "RuntimeEnabledFeatures.h"
#include "core/css/CSSVariablesMapForEachCallback.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class CSSStyleDeclaration;
class CSSVariablesIterator;
class ExceptionState;

class CSSVariablesMap FINAL : public RefCounted<CSSVariablesMap> {
public:
    static PassRefPtr<CSSVariablesMap> create(CSSStyleDeclaration* styleDeclaration)
    {
        return adoptRef(new CSSVariablesMap(styleDeclaration));
    }

    unsigned size() const;
    String get(const AtomicString& name) const;
    bool has(const AtomicString& name) const;
    void set(const AtomicString& name, const String& value, ExceptionState&);
    bool remove(const AtomicString& name);
    void clear(ExceptionState&);
    void forEach(PassRefPtr<CSSVariablesMapForEachCallback>, ScriptValue& thisArg) const;
    void forEach(PassRefPtr<CSSVariablesMapForEachCallback>) const;

    void clearStyleDeclaration() { m_styleDeclaration = 0; }

private:
    explicit CSSVariablesMap(CSSStyleDeclaration* styleDeclaration)
        : m_styleDeclaration(styleDeclaration)
    {
        ASSERT(RuntimeEnabledFeatures::cssVariablesEnabled());
    }

    void forEach(PassRefPtr<CSSVariablesMapForEachCallback>, ScriptValue* thisArg) const;

    CSSStyleDeclaration* m_styleDeclaration;
    typedef Vector<CSSVariablesIterator*> Iterators;
    mutable Iterators m_activeIterators;
};

} // namespace WebCore

#endif // CSSVariablesMap_h
