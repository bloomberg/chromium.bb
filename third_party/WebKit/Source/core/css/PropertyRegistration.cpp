// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/PropertyRegistration.h"

#include "core/css/CSSSyntaxDescriptor.h"
#include "core/css/CSSVariableReferenceValue.h"
#include "core/css/PropertyDescriptor.h"
#include "core/css/PropertyRegistry.h"
#include "core/css/parser/CSSVariableParser.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/StyleChangeReason.h"

namespace blink {

static bool computationallyIdempotent(const CSSValue& value)
{
    // TODO(timloh): Implement this
    return true;
}

void PropertyRegistration::registerProperty(ExecutionContext* executionContext, const PropertyDescriptor& descriptor, ExceptionState& exceptionState)
{
    // Bindings code ensures these are set.
    DCHECK(descriptor.hasName());
    DCHECK(descriptor.hasInherits());
    DCHECK(descriptor.hasSyntax());

    String name = descriptor.name();
    if (!CSSVariableParser::isValidVariableName(name)) {
        exceptionState.throwDOMException(SyntaxError, "The name provided is not a valid custom property name.");
        return;
    }
    AtomicString atomicName(name);
    Document* document = toDocument(executionContext);
    PropertyRegistry& registry = *document->propertyRegistry();
    if (registry.registration(atomicName)) {
        exceptionState.throwDOMException(InvalidModificationError, "The name provided has already been registered.");
        return;
    }

    CSSSyntaxDescriptor syntaxDescriptor(descriptor.syntax());
    if (!syntaxDescriptor.isValid()) {
        exceptionState.throwDOMException(SyntaxError, "The syntax provided is not a valid custom property syntax.");
        return;
    }

    if (descriptor.hasInitialValue()) {
        const CSSValue* initial = syntaxDescriptor.parse(descriptor.initialValue());
        if (!initial) {
            exceptionState.throwDOMException(SyntaxError, "The initial value provided does not parse for the given syntax.");
            return;
        }
        if (!computationallyIdempotent(*initial)) {
            exceptionState.throwDOMException(SyntaxError, "The initial value provided is not computationally idempotent.");
            return;
        }
        registry.registerProperty(atomicName, syntaxDescriptor, descriptor.inherits(), initial);
    } else {
        if (!syntaxDescriptor.isTokenStream()) {
            exceptionState.throwDOMException(SyntaxError, "An initial value must be provided if the syntax is not '*'");
            return;
        }
        registry.registerProperty(atomicName, syntaxDescriptor, descriptor.inherits(), nullptr);
    }

    // TODO(timloh): Invalidate only elements with this custom property set
    document->setNeedsStyleRecalc(SubtreeStyleChange, StyleChangeReasonForTracing::create(StyleChangeReason::PropertyRegistration));
}

void PropertyRegistration::unregisterProperty(ExecutionContext* executionContext, const String& property, ExceptionState& exceptionState)
{
    Document* document = toDocument(executionContext);
    PropertyRegistry& registry = *document->propertyRegistry();
    AtomicString atomicProperty(property);
    if (!registry.registration(atomicProperty)) {
        exceptionState.throwDOMException(NotFoundError, "CSS.unregisterProperty() called with non-registered property " + property);
        return;
    }
    registry.unregisterProperty(atomicProperty);

    document->setNeedsStyleRecalc(SubtreeStyleChange, StyleChangeReasonForTracing::create(StyleChangeReason::PropertyUnregistration));
}

} // namespace blink
