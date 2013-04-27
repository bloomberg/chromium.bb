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
#include "core/dom/ContextDestructionObserver.h"
#include "core/dom/Document.h"
#include "core/dom/QualifiedName.h"
#include <wtf/Forward.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

class CustomElementConstructor;
class Document;
class Element;
class ScriptState;

PassRefPtr<Element> setTypeExtension(PassRefPtr<Element>, const AtomicString& typeExtension);

class CustomElementDefinition : public RefCounted<CustomElementDefinition> , public ContextDestructionObserver {
public:
    static PassRefPtr<CustomElementDefinition> create(ScriptState*, Document*, const QualifiedName& typeName, const QualifiedName& localName, const ScriptValue& prototype);

    virtual ~CustomElementDefinition() {}

    Document* document() const { return static_cast<Document*>(m_scriptExecutionContext); }
    const QualifiedName& typeName() const { return m_typeName; }
    const QualifiedName& localName() const { return m_localName; }
    bool isExtended() const { return m_typeName != m_localName; }

    const ScriptValue& prototype() { return m_prototype; }
    PassRefPtr<Element> createElement();

private:
    CustomElementDefinition(Document*, const QualifiedName& typeName, const QualifiedName& localName, const ScriptValue& prototype);

    PassRefPtr<Element> createElementInternal();

    ScriptValue m_prototype;
    QualifiedName m_typeName;
    QualifiedName m_localName;
};

}

#endif // CustomElementDefinition_h
