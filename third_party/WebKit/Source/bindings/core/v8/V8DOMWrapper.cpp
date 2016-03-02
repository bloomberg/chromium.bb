/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "bindings/core/v8/V8DOMWrapper.h"

#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8HTMLCollection.h"
#include "bindings/core/v8/V8HTMLDocument.h"
#include "bindings/core/v8/V8Location.h"
#include "bindings/core/v8/V8ObjectConstructor.h"
#include "bindings/core/v8/V8PerContextData.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "bindings/core/v8/V8Window.h"

namespace blink {

static v8::Local<v8::Object> wrapInShadowTemplate(v8::Local<v8::Object> wrapper, ScriptWrappable* scriptWrappable, v8::Isolate* isolate)
{
    static int shadowTemplateKey; // This address is used for a key to look up the dom template.
    V8PerIsolateData* data = V8PerIsolateData::from(isolate);
    v8::Local<v8::FunctionTemplate> shadowTemplate = data->existingDOMTemplate(&shadowTemplateKey);
    if (shadowTemplate.IsEmpty()) {
        shadowTemplate = v8::FunctionTemplate::New(isolate);
        if (shadowTemplate.IsEmpty())
            return v8::Local<v8::Object>();
        shadowTemplate->SetClassName(v8AtomicString(isolate, "HTMLDocument"));
        shadowTemplate->Inherit(V8HTMLDocument::domTemplate(isolate));
        shadowTemplate->InstanceTemplate()->SetInternalFieldCount(V8HTMLDocument::internalFieldCount);
        data->setDOMTemplate(&shadowTemplateKey, shadowTemplate);
    }

    v8::Local<v8::Function> shadowConstructor;
    if (!shadowTemplate->GetFunction(isolate->GetCurrentContext()).ToLocal(&shadowConstructor))
        return v8::Local<v8::Object>();

    v8::Local<v8::Object> shadow;
    if (!V8ScriptRunner::instantiateObject(isolate, shadowConstructor).ToLocal(&shadow))
        return v8::Local<v8::Object>();
    if (!v8CallBoolean(shadow->SetPrototype(isolate->GetCurrentContext(), wrapper)))
        return v8::Local<v8::Object>();
    V8DOMWrapper::setNativeInfo(wrapper, &V8HTMLDocument::wrapperTypeInfo, scriptWrappable);
    return shadow;
}

v8::Local<v8::Object> V8DOMWrapper::createWrapper(v8::Isolate* isolate, v8::Local<v8::Object> creationContext, const WrapperTypeInfo* type, ScriptWrappable* scriptWrappable)
{
    ASSERT(!type->equals(&V8Window::wrapperTypeInfo));
    // According to https://html.spec.whatwg.org/multipage/browsers.html#security-location,
    // cross-origin script access to a few properties of Location is allowed.
    // Location already implements the necessary security checks.
    bool withSecurityCheck = !type->equals(&V8Location::wrapperTypeInfo);
    V8WrapperInstantiationScope scope(creationContext, isolate, withSecurityCheck);

    V8PerContextData* perContextData = V8PerContextData::from(scope.context());
    v8::Local<v8::Object> wrapper;
    if (perContextData) {
        wrapper = perContextData->createWrapperFromCache(type);
    } else {
        v8::Local<v8::Function> function;
        if (!type->domTemplate(isolate)->GetFunction(isolate->GetCurrentContext()).ToLocal(&function))
            return v8::Local<v8::Object>();
        if (!V8ObjectConstructor::newInstance(isolate, function).ToLocal(&wrapper))
            return v8::Local<v8::Object>();
    }

    if (type == &V8HTMLDocument::wrapperTypeInfo && !wrapper.IsEmpty())
        wrapper = wrapInShadowTemplate(wrapper, scriptWrappable, isolate);

    return wrapper;
}

bool V8DOMWrapper::isWrapper(v8::Isolate* isolate, v8::Local<v8::Value> value)
{
    if (value.IsEmpty() || !value->IsObject())
        return false;
    v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(value);

    if (object->InternalFieldCount() < v8DefaultWrapperInternalFieldCount)
        return false;

    const WrapperTypeInfo* untrustedWrapperTypeInfo = toWrapperTypeInfo(object);
    V8PerIsolateData* perIsolateData = V8PerIsolateData::from(isolate);
    if (!(untrustedWrapperTypeInfo && perIsolateData))
        return false;
    return perIsolateData->hasInstance(untrustedWrapperTypeInfo, object);
}

bool V8DOMWrapper::hasInternalFieldsSet(v8::Local<v8::Value> value)
{
    if (value.IsEmpty() || !value->IsObject())
        return false;
    v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(value);

    if (object->InternalFieldCount() < v8DefaultWrapperInternalFieldCount)
        return false;

    const ScriptWrappable* untrustedScriptWrappable = toScriptWrappable(object);
    const WrapperTypeInfo* untrustedWrapperTypeInfo = toWrapperTypeInfo(object);
    return untrustedScriptWrappable
        && untrustedWrapperTypeInfo
        && untrustedWrapperTypeInfo->ginEmbedder == gin::kEmbedderBlink;
}

void V8WrapperInstantiationScope::securityCheck(v8::Isolate* isolate, v8::Local<v8::Context> contextForWrapper)
{
    if (m_context.IsEmpty())
        return;
    // If the context is different, we need to make sure that the current
    // context has access to the creation context.
    Frame* frame = toFrameIfNotDetached(contextForWrapper);
    if (!frame)
        return;
    const DOMWrapperWorld& currentWorld = DOMWrapperWorld::world(m_context);
    RELEASE_ASSERT(currentWorld.worldId() == DOMWrapperWorld::world(contextForWrapper).worldId());
    if (currentWorld.isMainWorld()) {
        RELEASE_ASSERT(BindingSecurity::shouldAllowAccessToFrame(isolate, callingDOMWindow(isolate), frame, DoNotReportSecurityError));
    }
}

void V8WrapperInstantiationScope::convertException()
{
    v8::Isolate* isolate = m_context->GetIsolate();
    // TODO(jochen): Currently, Location is the only object for which we can reach this code path. Should be generalized.
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "Location", isolate->GetCurrentContext()->Global(), isolate);
    LocalDOMWindow* callingWindow = callingDOMWindow(isolate);
    DOMWindow* targetWindow = toDOMWindow(m_context);
    exceptionState.throwSecurityError(targetWindow->sanitizedCrossDomainAccessErrorMessage(callingWindow), targetWindow->crossDomainAccessErrorMessage(callingWindow));
    exceptionState.throwIfNeeded();
}

} // namespace blink
