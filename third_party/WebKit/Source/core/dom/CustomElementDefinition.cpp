/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#include "config.h"

#include "core/dom/CustomElementDefinition.h"

#include "bindings/v8/CustomElementHelpers.h"
#include <wtf/Assertions.h>

#if ENABLE(SVG)
#include "SVGNames.h"
#endif

namespace WebCore {

PassRefPtr<CustomElementDefinition> CustomElementDefinition::create(ScriptState* state, const AtomicString& type, const AtomicString& name, const AtomicString& namespaceURI, const ScriptValue& prototype)
{
    ASSERT(CustomElementHelpers::isValidPrototypeParameter(prototype, state));
    ASSERT(name == type || QualifiedName(nullAtom, name, namespaceURI) == *CustomElementHelpers::findLocalName(prototype));
#if ENABLE(SVG)
    ASSERT(namespaceURI == HTMLNames::xhtmlNamespaceURI || namespaceURI == SVGNames::svgNamespaceURI);
#else
    ASSERT(namespaceURI == HTMLNames::xhtmlNamespaceURI);
#endif

    RefPtr<CustomElementDefinition> created = adoptRef(new CustomElementDefinition(type, name, namespaceURI, prototype));
    return created.release();
}

CustomElementDefinition::CustomElementDefinition(const AtomicString& type, const AtomicString& name, const AtomicString& namespaceURI, const ScriptValue& prototype)
    : m_prototype(prototype)
    , m_type(type)
    , m_tag(QualifiedName(nullAtom, name, namespaceURI))
{
}

}
