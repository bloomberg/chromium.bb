/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#ifndef CustomElementRegistry_h
#define CustomElementRegistry_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/CustomElementUpgradeCandidateMap.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/QualifiedName.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/AtomicStringHash.h"

namespace WebCore {

class CustomElementConstructorBuilder;
class CustomElementDefinition;
class Document;
class Element;

void setTypeExtension(Element*, const AtomicString& typeExtension);

class CustomElementRegistry : public RefCounted<CustomElementRegistry>, public ContextLifecycleObserver {
    WTF_MAKE_NONCOPYABLE(CustomElementRegistry); WTF_MAKE_FAST_ALLOCATED;
public:
    explicit CustomElementRegistry(Document*);
    virtual ~CustomElementRegistry() { }

    void registerElement(CustomElementConstructorBuilder*, const AtomicString& name, ExceptionCode&);

    PassRefPtr<CustomElementDefinition> findFor(Element*) const;

    PassRefPtr<Element> createCustomTagElement(const QualifiedName& localName);

    Document* document() const;

    void didGiveTypeExtension(Element*, const AtomicString&);
    void customElementWasDestroyed(Element*);

    static bool isCustomTagName(const AtomicString& name) { return isValidName(name); }

private:
    typedef HashMap<AtomicString, RefPtr<CustomElementDefinition> > DefinitionMap;
    static bool isValidName(const AtomicString&);

    PassRefPtr<CustomElementDefinition> findAndCheckNamespace(const AtomicString& type, const AtomicString& namespaceURI) const;

    void didCreateCustomTagElement(CustomElementDefinition*, Element*);
    void didCreateUnresolvedElement(CustomElementDefinition::CustomElementKind, const AtomicString& type, Element*);

    DefinitionMap m_definitions;
    CustomElementUpgradeCandidateMap m_candidates;
};

} // namespace WebCore

#endif
