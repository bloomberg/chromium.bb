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

#ifndef CustomElementConstructorBuilder_h
#define CustomElementConstructorBuilder_h

#include "bindings/v8/ScriptValue.h"
#include "core/dom/CustomElementCallback.h"
#include "core/dom/QualifiedName.h"
#include "wtf/HashSet.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/text/AtomicString.h"
#include <v8.h>

namespace WebCore {

class CustomElementDefinition;
class Dictionary;
class Document;
class Element;
class QualifiedName;
class ScriptState;
class WrapperTypeInfo;

// Handles the scripting-specific parts of the Custom Elements element
// registration algorithm and constructor generation algorithm. It is
// used in the implementation of those algorithms in
// Document::registerElement.
class CustomElementConstructorBuilder {
    WTF_MAKE_NONCOPYABLE(CustomElementConstructorBuilder);
public:
    CustomElementConstructorBuilder(ScriptState*, const Dictionary* options);

    // The builder accumulates state and may run script at specific
    // points. These methods must be called in order. When one fails
    // (returns false), the calls must stop.

    bool isFeatureAllowed() const;
    bool validateOptions();
    bool findTagName(const AtomicString& customElementType, QualifiedName& tagName) const;
    PassRefPtr<CustomElementCallback> createCallback(Document*);
    bool createConstructor(Document*, CustomElementDefinition*);
    void didRegisterDefinition(CustomElementDefinition*, const HashSet<Element*>& upgradeCandidates) const;

    // This method collects a return value for the bindings. It is
    // safe to call this method even if the builder failed; it will
    // return an empty value.
    ScriptValue bindingsReturnValue() const;

private:
    bool hasValidPrototypeChainFor(WrapperTypeInfo*) const;
    bool prototypeIsValid() const;

    v8::Handle<v8::Context> m_context;
    const Dictionary* m_options;
    v8::Handle<v8::Object> m_prototype;
    AtomicString m_namespaceURI;
    v8::Handle<v8::Function> m_constructor;
};

}

#endif // CustomElementConstructorBuilder_h
