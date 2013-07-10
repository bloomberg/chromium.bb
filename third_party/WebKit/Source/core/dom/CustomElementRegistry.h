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

#include "core/dom/CustomElementDefinition.h"
#include "core/dom/CustomElementDescriptor.h"
#include "core/dom/CustomElementUpgradeCandidateMap.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/QualifiedName.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/AtomicStringHash.h"

namespace WebCore {

class CustomElementConstructorBuilder;
class Document;
class Element;

// Element creation
void setTypeExtension(Element*, const AtomicString& typeExtension);

class CustomElementRegistry : public RefCounted<CustomElementRegistry> {
    WTF_MAKE_NONCOPYABLE(CustomElementRegistry); WTF_MAKE_FAST_ALLOCATED;
public:
    CustomElementRegistry() { }
    virtual ~CustomElementRegistry() { }

    // Model
    static bool isCustomTagName(const AtomicString& name) { return isValidTypeName(name); }

    // Registration and definitions
    void registerElement(Document*, CustomElementConstructorBuilder*, const AtomicString& name, ExceptionCode&);
    CustomElementDefinition* findFor(Element*) const;

    // Element creation
    PassRefPtr<Element> createCustomTagElement(Document*, const QualifiedName& localName);
    void didGiveTypeExtension(Element*, const AtomicString&);

    // Lifecycle
    void customElementWasDestroyed(Element*);

private:
    // Model
    static bool isValidTypeName(const AtomicString&);
    CustomElementDescriptor describe(Element*) const;

    // Registration and definitions
    CustomElementDefinition* find(const CustomElementDescriptor&) const;

    // Lifecycle
    void didCreateUnresolvedElement(const CustomElementDescriptor&, Element*);
    void didResolveElement(CustomElementDefinition*, Element*) const;

    // Registration and definitions
    typedef HashMap<CustomElementDescriptor, RefPtr<CustomElementDefinition> > DefinitionMap;
    DefinitionMap m_definitions;
    HashSet<AtomicString> m_registeredTypeNames;

    // Element creation
    typedef HashMap<Element*, AtomicString> ElementTypeMap;
    ElementTypeMap m_elementTypeMap; // Creation-time "is" attribute value.
    CustomElementUpgradeCandidateMap m_candidates;
};

} // namespace WebCore

#endif
