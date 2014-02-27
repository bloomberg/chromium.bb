/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "bindings/v8/V8PerIsolateData.h"

#include "bindings/v8/DOMDataStore.h"
#include "bindings/v8/ScriptGCEvent.h"
#include "bindings/v8/ScriptProfiler.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8ObjectConstructor.h"
#include "bindings/v8/V8ScriptRunner.h"
#include "gin/public/isolate_holder.h"
#include "wtf/MainThread.h"

namespace WebCore {

static gin::IsolateHolder* mainIsolateHolder = 0;

V8PerIsolateData::V8PerIsolateData(v8::Isolate* isolate)
    : m_isolate(isolate)
    , m_isMainThread(WTF::isMainThread())
    , m_stringCache(adoptPtr(new StringCache()))
    , m_constructorMode(ConstructorMode::CreateNewObject)
    , m_recursionLevel(0)
#ifndef NDEBUG
    , m_internalScriptRecursionLevel(0)
#endif
    , m_gcEventData(adoptPtr(new GCEventData()))
    , m_performingMicrotaskCheckpoint(false)
{
    if (m_isMainThread)
        mainIsolateHolder = new gin::IsolateHolder(m_isolate, v8ArrayBufferAllocator());
}

V8PerIsolateData::~V8PerIsolateData()
{
    if (m_isMainThread) {
        delete mainIsolateHolder;
        mainIsolateHolder = 0;
    }
}

v8::Isolate* V8PerIsolateData::mainThreadIsolate()
{
    ASSERT(WTF::isMainThread());
    ASSERT(mainIsolateHolder);
    return mainIsolateHolder->isolate();
}

void V8PerIsolateData::ensureInitialized(v8::Isolate* isolate)
{
    ASSERT(isolate);
    if (!isolate->GetData(gin::kEmbedderBlink)) {
        V8PerIsolateData* data = new V8PerIsolateData(isolate);
        isolate->SetData(gin::kEmbedderBlink, data);
    }
}

v8::Persistent<v8::Value>& V8PerIsolateData::ensureLiveRoot()
{
    if (m_liveRoot.isEmpty())
        m_liveRoot.set(m_isolate, v8::Null(m_isolate));
    return m_liveRoot.getUnsafe();
}

void V8PerIsolateData::dispose(v8::Isolate* isolate)
{
    void* data = isolate->GetData(gin::kEmbedderBlink);
    delete static_cast<V8PerIsolateData*>(data);
    isolate->SetData(gin::kEmbedderBlink, 0);
}

v8::Handle<v8::FunctionTemplate> V8PerIsolateData::domTemplate(WrapperWorldType currentWorldType, void* domTemplateKey, v8::FunctionCallback callback, v8::Handle<v8::Value> data, v8::Handle<v8::Signature> signature, int length)
{
    TemplateMap& templates = templateMap(currentWorldType);
    TemplateMap::iterator result = templates.find(domTemplateKey);
    if (result != templates.end())
        return result->value.newLocal(m_isolate);
    v8::Local<v8::FunctionTemplate> templ = v8::FunctionTemplate::New(m_isolate, callback, data, signature, length);
    templates.add(domTemplateKey, UnsafePersistent<v8::FunctionTemplate>(m_isolate, templ));
    return templ;
}

v8::Handle<v8::FunctionTemplate> V8PerIsolateData::existingDOMTemplate(WrapperWorldType currentWorldType, void* domTemplateKey)
{
    TemplateMap& templates = templateMap(currentWorldType);
    TemplateMap::iterator result = templates.find(domTemplateKey);
    if (result != templates.end())
        return result->value.newLocal(m_isolate);
    return v8::Local<v8::FunctionTemplate>();
}

void V8PerIsolateData::setDOMTemplate(WrapperWorldType currentWorldType, void* domTemplateKey, v8::Handle<v8::FunctionTemplate> templ)
{
    templateMap(currentWorldType).add(domTemplateKey, UnsafePersistent<v8::FunctionTemplate>(m_isolate, templ));
}

v8::Local<v8::Context> V8PerIsolateData::ensureRegexContext()
{
    if (m_regexContext.isEmpty())
        m_regexContext.set(m_isolate, v8::Context::New(m_isolate));
    return m_regexContext.newLocal(m_isolate);
}

bool V8PerIsolateData::hasInstance(const WrapperTypeInfo* info, v8::Handle<v8::Value> value)
{
    return hasInstance(info, value, m_templatesForMainWorld)
        || hasInstance(info, value, m_templatesForNonMainWorld);
}

bool V8PerIsolateData::hasInstance(const WrapperTypeInfo* info, v8::Handle<v8::Value> value, TemplateMap& templates)
{
    TemplateMap::iterator result = templates.find(info);
    if (result == templates.end())
        return false;
    v8::Handle<v8::FunctionTemplate> templ = result->value.newLocal(m_isolate);
    return templ->HasInstance(value);
}

v8::Handle<v8::Object> V8PerIsolateData::findInstanceInPrototypeChain(const WrapperTypeInfo* info, v8::Handle<v8::Value> value)
{
    v8::Handle<v8::Object> wrapper = findInstanceInPrototypeChain(info, value, m_templatesForMainWorld);
    if (!wrapper.IsEmpty())
        return wrapper;
    return findInstanceInPrototypeChain(info, value, m_templatesForNonMainWorld);
}

v8::Handle<v8::Object> V8PerIsolateData::findInstanceInPrototypeChain(const WrapperTypeInfo* info, v8::Handle<v8::Value> value, TemplateMap& templates)
{
    if (value.IsEmpty() || !value->IsObject())
        return v8::Handle<v8::Object>();
    TemplateMap::iterator result = templates.find(info);
    if (result == templates.end())
        return v8::Handle<v8::Object>();
    v8::Handle<v8::FunctionTemplate> templ = result->value.newLocal(m_isolate);
    return v8::Handle<v8::Object>::Cast(value)->FindInstanceInPrototypeChain(templ);
}

static void constructorOfToString(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    // The DOM constructors' toString functions grab the current toString
    // for Functions by taking the toString function of itself and then
    // calling it with the constructor as its receiver. This means that
    // changes to the Function prototype chain or toString function are
    // reflected when printing DOM constructors. The only wart is that
    // changes to a DOM constructor's toString's toString will cause the
    // toString of the DOM constructor itself to change. This is extremely
    // obscure and unlikely to be a problem.
    v8::Handle<v8::Value> value = info.Callee()->Get(v8AtomicString(info.GetIsolate(), "toString"));
    if (!value->IsFunction()) {
        v8SetReturnValue(info, v8::String::Empty(info.GetIsolate()));
        return;
    }
    v8SetReturnValue(info, V8ScriptRunner::callInternalFunction(v8::Handle<v8::Function>::Cast(value), info.This(), 0, 0, v8::Isolate::GetCurrent()));
}

v8::Handle<v8::FunctionTemplate> V8PerIsolateData::toStringTemplate()
{
    if (m_toStringTemplate.isEmpty())
        m_toStringTemplate.set(m_isolate, v8::FunctionTemplate::New(m_isolate, constructorOfToString));
    return m_toStringTemplate.newLocal(m_isolate);
}

} // namespace WebCore
