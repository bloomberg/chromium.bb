/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef CustomElementRegistrationContext_h
#define CustomElementRegistrationContext_h

#include "core/dom/CustomElementDescriptor.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/QualifiedName.h"
#include "wtf/PassRefPtr.h"
#include "wtf/text/AtomicString.h"

namespace WebCore {

class CustomElementConstructorBuilder;
class Document;
class Element;

class CustomElementRegistrationContext : public RefCounted<CustomElementRegistrationContext> {
public:
    static PassRefPtr<CustomElementRegistrationContext> nullRegistrationContext();
    static PassRefPtr<CustomElementRegistrationContext> create();

    virtual ~CustomElementRegistrationContext() { }

    // Model
    // FIXME: Move this to CustomElementRegistry
    static bool isValidTypeName(const AtomicString& type);
    // FIXME: Move this to CustomElement
    static bool isCustomTagName(const AtomicString& localName);

    // Definitions
    virtual void registerElement(Document*, CustomElementConstructorBuilder*, const AtomicString& type, ExceptionCode&) = 0;

    // Instance creation
    virtual PassRefPtr<Element> createCustomTagElement(Document*, const QualifiedName&) = 0;
    static void setIsAttributeAndTypeExtension(Element*, const AtomicString& type);
    static void setTypeExtension(Element*, const AtomicString& type);

    // Instance lifecycle
    virtual void customElementWasDestroyed(Element*) = 0;

protected:
    CustomElementRegistrationContext() { }

    // Instance creation
    virtual void didGiveTypeExtension(Element*, const AtomicString& type) = 0;
};

}

#endif // CustomElementRegistrationContext_h

