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

#include "config.h"
#include "core/dom/CustomElementRegistry.h"

#include "HTMLNames.h"
#include "SVGNames.h"
#include "bindings/v8/CustomElementConstructorBuilder.h"
#include "core/dom/CustomElement.h"
#include "core/dom/CustomElementDefinition.h"
#include "core/dom/CustomElementException.h"
#include "core/dom/CustomElementRegistrationContext.h"
#include "core/dom/DocumentLifecycleObserver.h"

namespace WebCore {

class RegistrationContextObserver : public DocumentLifecycleObserver {
public:
    explicit RegistrationContextObserver(Document* document)
        : DocumentLifecycleObserver(document)
        , m_wentAway(!document)
    {
    }

    bool registrationContextWentAway() { return m_wentAway; }

private:
    virtual void documentWasDisposed() OVERRIDE { m_wentAway = true; }

    bool m_wentAway;
};

CustomElementDefinition* CustomElementRegistry::registerElement(Document* document, CustomElementConstructorBuilder* constructorBuilder, const AtomicString& userSuppliedName, CustomElement::NameSet validNames, ExceptionState& es)
{
    // FIXME: In every instance except one it is the
    // CustomElementConstructorBuilder that observes document
    // destruction during registration. This responsibility should be
    // consolidated in one place.
    RegistrationContextObserver observer(document);

    AtomicString type = userSuppliedName.lower();

    if (!constructorBuilder->isFeatureAllowed()) {
        CustomElementException::throwException(CustomElementException::CannotRegisterFromExtension, type, es);
        return 0;
    }

    if (!CustomElement::isValidName(type, validNames)) {
        CustomElementException::throwException(CustomElementException::InvalidName, type, es);
        return 0;
    }

    QualifiedName tagName = nullQName();
    if (!constructorBuilder->validateOptions(type, tagName, es))
        return 0;

    ASSERT(tagName.namespaceURI() == HTMLNames::xhtmlNamespaceURI || tagName.namespaceURI() == SVGNames::svgNamespaceURI);

    // FIXME: This should be done earlier in validateOptions.
    if (m_registeredTypeNames.contains(type)) {
        CustomElementException::throwException(CustomElementException::TypeAlreadyRegistered, type, es);
        return 0;
    }

    ASSERT(!observer.registrationContextWentAway());

    RefPtr<CustomElementLifecycleCallbacks> lifecycleCallbacks = constructorBuilder->createCallbacks();

    // Consulting the constructor builder could execute script and
    // kill the document.
    if (observer.registrationContextWentAway()) {
        CustomElementException::throwException(CustomElementException::ContextDestroyedCreatingCallbacks, type, es);
        return 0;
    }

    const CustomElementDescriptor descriptor(type, tagName.namespaceURI(), tagName.localName());
    RefPtr<CustomElementDefinition> definition = CustomElementDefinition::create(descriptor, lifecycleCallbacks);

    if (!constructorBuilder->createConstructor(document, definition.get(), es))
        return 0;

    m_definitions.add(descriptor, definition);
    m_registeredTypeNames.add(descriptor.type());

    if (!constructorBuilder->didRegisterDefinition(definition.get())) {
        CustomElementException::throwException(CustomElementException::ContextDestroyedRegisteringDefinition, type, es);
        return 0;
    }

    return definition.get();
}

CustomElementDefinition* CustomElementRegistry::find(const CustomElementDescriptor& descriptor) const
{
    return m_definitions.get(descriptor);
}

} // namespace WebCore
