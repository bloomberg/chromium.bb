/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/css/CSSVariablesMap.h"

#include "bindings/v8/ExceptionState.h"
#include "core/css/CSSStyleDeclaration.h"

namespace WebCore {

unsigned CSSVariablesMap::size() const
{
    if (m_styleDeclaration)
        return m_styleDeclaration->variableCount();
    return 0;
}

String CSSVariablesMap::get(const AtomicString& name) const
{
    if (m_styleDeclaration)
        return m_styleDeclaration->variableValue(name);
    return String();
}

bool CSSVariablesMap::has(const AtomicString& name) const
{
    if (m_styleDeclaration)
        return !get(name).isEmpty();
    return false;
}

void CSSVariablesMap::set(const AtomicString& name, const String& value, ExceptionState& es) const
{
    if (m_styleDeclaration)
        m_styleDeclaration->setVariableValue(name, value, es);
}

bool CSSVariablesMap::remove(const AtomicString& name) const
{
    if (m_styleDeclaration)
        return m_styleDeclaration->removeVariable(name);
    return false;
}

void CSSVariablesMap::clear(ExceptionState& es) const
{
    if (m_styleDeclaration)
        return m_styleDeclaration->clearVariables(es);
}

} // namespace WebCore
