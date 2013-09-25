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

#include "config.h"
#include "core/dom/CustomElementException.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"

namespace WebCore {

String CustomElementException::preamble(const AtomicString& type)
{
    return "Failed to call 'register' on 'Document' for type '" + type + "': ";
}

void CustomElementException::throwException(Reason reason, const AtomicString& type, ExceptionState& es)
{
    switch (reason) {
    case CannotRegisterFromExtension:
        es.throwDOMException(NotSupportedError, preamble(type) + "elements cannot be registered from extensions.");
        return;

    case ConstructorPropertyNotConfigurable:
        es.throwDOMException(NotSupportedError, preamble(type) + "prototype constructor property is not configurable.");
        return;

    case ContextDestroyedCheckingPrototype:
        es.throwDOMException(InvalidStateError, preamble(type) + "the context is no longer valid.");
        return;

    case ContextDestroyedCreatingCallbacks:
        es.throwUninformativeAndGenericDOMException(InvalidStateError);
        return;

    case ContextDestroyedRegisteringDefinition:
        es.throwUninformativeAndGenericDOMException(NotSupportedError);
        return;

    case ExtendsIsInvalidName:
        es.throwDOMException(InvalidCharacterError, preamble(type) + ": the tag name specified in 'extends' is not a valid tag name.");
        return;

    case ExtendsIsCustomElementName:
        es.throwDOMException(InvalidCharacterError, preamble(type) + ": the tag name specified in 'extends' is a custom element name. Use inheritance instead.");
        return;

    case InvalidName:
        es.throwDOMException(InvalidCharacterError, preamble(type) + ": '" + type + "' is not a valid name.");
        return;

    case PrototypeInUse:
        es.throwDOMException(NotSupportedError, preamble(type) + "prototype is already in-use as an interface prototype object.");
        return;

    case PrototypeNotAnObject:
        es.throwDOMException(InvalidStateError, preamble(type) + "the prototype option is not an object.");
        return;

    case TypeAlreadyRegistered:
        es.throwDOMException(InvalidStateError, preamble(type) + "a type with that name is already registered.");
        return;
    }

    ASSERT_NOT_REACHED();
}

} // namespace WebCore
