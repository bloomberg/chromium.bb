// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8HTMLElement.h"

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingMacros.h"
#include "bindings/core/v8/V8DOMWrapper.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/dom/Document.h"
#include "core/dom/custom/CustomElementDefinition.h"
#include "core/dom/custom/CustomElementsRegistry.h"
#include "core/frame/LocalDOMWindow.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

void V8HTMLElement::constructorCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info)
{
    DCHECK(info.IsConstructCall());

    v8::Isolate* isolate = info.GetIsolate();
    ScriptState* scriptState = ScriptState::current(isolate);

    if (!RuntimeEnabledFeatures::customElementsV1Enabled()
        || !scriptState->world().isMainWorld()) {
        V8ThrowException::throwTypeError(info.GetIsolate(), "Illegal constructor");
        return;
    }

    LocalDOMWindow* window = scriptState->domWindow();
    CustomElementDefinition* def =
        window->customElements(scriptState)->definitionForConstructor(
            scriptState,
            info.NewTarget());
    if (!def) {
        V8ThrowException::throwTypeError(isolate, "Illegal constructor");
        return;
    }

    // TODO(dominicc): Implement cases where the definition's
    // construction stack is not empty when parser-creation is
    // implemented.

    ExceptionState exceptionState(
        ExceptionState::ConstructionContext,
        "HTMLElement",
        info.Holder(),
        isolate);
    Element* element = window->document()->createElement(
        def->localName(),
        AtomicString(),
        exceptionState);
    if (exceptionState.throwIfNeeded())
        return;
    const WrapperTypeInfo* wrapperType = element->wrapperTypeInfo();
    v8::Local<v8::Object> wrapper = V8DOMWrapper::associateObjectWithWrapper(
        isolate,
        element,
        wrapperType,
        info.This());

    if (!v8CallBoolean(wrapper->SetPrototype(
        scriptState->context(),
        def->prototype(scriptState)))) {
        return;
    }
}

} // namespace blink
