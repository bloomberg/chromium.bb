// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementsRegistry.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptCustomElementDefinitionBuilder.h"
#include "core/dom/Document.h"
#include "core/dom/ElementRegistrationOptions.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/custom/CustomElement.h"
#include "core/dom/custom/CustomElementDefinition.h"
#include "core/dom/custom/CustomElementDefinitionBuilder.h"
#include "core/dom/custom/V0CustomElementRegistrationContext.h"

namespace blink {

CustomElementsRegistry* CustomElementsRegistry::create(
    V0CustomElementRegistrationContext* v0)
{
    // TODO(dominicc): The window could install a new document; add a signal
    // when a window installs a new document to notify that V0 context, too.
    CustomElementsRegistry* registry = new CustomElementsRegistry(v0);
    if (v0)
        v0->setV1(registry);
    return registry;
}

CustomElementsRegistry::CustomElementsRegistry(
    const V0CustomElementRegistrationContext* v0)
    : m_v0(v0)
{
}

DEFINE_TRACE(CustomElementsRegistry)
{
    visitor->trace(m_definitions);
    visitor->trace(m_v0);
}

void CustomElementsRegistry::define(
    ScriptState* scriptState,
    const AtomicString& name,
    const ScriptValue& constructor,
    const ElementRegistrationOptions& options,
    ExceptionState& exceptionState)
{
    ScriptCustomElementDefinitionBuilder builder(
        scriptState,
        this,
        constructor,
        exceptionState);
    define(name, builder, options, exceptionState);
}

// http://w3c.github.io/webcomponents/spec/custom/#dfn-element-definition
void CustomElementsRegistry::define(
    const AtomicString& name,
    CustomElementDefinitionBuilder& builder,
    const ElementRegistrationOptions& options,
    ExceptionState& exceptionState)
{
    if (!builder.checkConstructorIntrinsics())
        return;

    if (!CustomElement::isValidName(name)) {
        exceptionState.throwDOMException(
            SyntaxError,
            "\"" + name + "\" is not a valid custom element name");
        return;
    }

    if (nameIsDefined(name) || v0NameIsDefined(name)) {
        exceptionState.throwDOMException(
            NotSupportedError,
            "this name has already been used with this registry");
        return;
    }

    if (!builder.checkConstructorNotRegistered())
        return;

    // TODO(dominicc): Implement steps:
    // 5: localName
    // 6-7: extends processing
    // 8-9: observed attributes caching

    // TODO(dominicc): Add a test where the prototype getter destroys
    // the context.

    if (!builder.checkPrototype())
        return;

    // TODO(dominicc): Implement steps:
    // 12-13: connected callback
    // 14-15: disconnected callback
    // 16-17: attribute changed callback

    // TODO(dominicc): Add a test where retrieving the prototype
    // recursively calls define with the same name.

    CustomElementDescriptor descriptor(name, name);
    CustomElementDefinition* definition = builder.build(descriptor);
    CHECK(!exceptionState.hadException());
    CHECK(definition->descriptor() == descriptor);
    DefinitionMap::AddResult result =
        m_definitions.add(descriptor.name(), definition);
    CHECK(result.isNewEntry);

    // TODO(dominicc): Implement steps:
    // 20: when-defined promise processing
}

bool CustomElementsRegistry::v0NameIsDefined(const AtomicString& name) const
{
    if (!m_v0)
        return false;
    return m_v0->nameIsDefined(name);
}

CustomElementDefinition* CustomElementsRegistry::definitionForName(
    const AtomicString& name) const
{
    return m_definitions.get(name);
}

} // namespace blink
