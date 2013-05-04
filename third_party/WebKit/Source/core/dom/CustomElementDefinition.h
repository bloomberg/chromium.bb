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

#ifndef CustomElementDefinition_h
#define CustomElementDefinition_h

#include "bindings/v8/ScriptValue.h"
#include "core/dom/QualifiedName.h"
#include "wtf/Forward.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class ScriptState;

class CustomElementDefinition : public RefCounted<CustomElementDefinition> {
public:
    static PassRefPtr<CustomElementDefinition> create(ScriptState*, const AtomicString& type, const AtomicString& name, const AtomicString& namespaceURI, const ScriptValue& prototype);

    virtual ~CustomElementDefinition() {}

    enum CustomElementKind {
        CustomTag,
        TypeExtension
    };

    // This specifies whether the custom element is in the HTML or SVG
    // namespace.
    const AtomicString& namespaceURI() const { return m_tag.namespaceURI(); }

    // This uniquely identifies the Custom Element. For custom tags, this
    // is the tag name and the same as "name". For type extensions, this
    // is the value of the "is" attribute.
    const AtomicString& type() const { return m_type; }

    // This is the tag name of the Custom Element.
    const AtomicString& name() const { return m_tag.localName(); }

    // This is a convenience property derived from "namespaceURI" and
    // "name". Custom Elements of this kind will have this tag
    // name. This does not have a prefix.
    const QualifiedName& tagQName() const { return m_tag; }

    CustomElementKind kind() const { return isTypeExtension() ? TypeExtension : CustomTag; }
    bool isTypeExtension() const { return type() != name(); }

    const ScriptValue& prototype() { return m_prototype; }

private:
    CustomElementDefinition(const AtomicString& type, const AtomicString& name, const AtomicString& namespaceURI, const ScriptValue& prototype);

    ScriptValue m_prototype;

    AtomicString m_type;
    QualifiedName m_tag;
};

}

#endif // CustomElementDefinition_h
