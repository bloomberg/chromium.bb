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

#include "bindings/core/v8/V8PerIsolateData.h"

#include "bindings/core/v8/DOMDataStore.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8HiddenValue.h"
#include "bindings/core/v8/V8ObjectConstructor.h"
#include "bindings/core/v8/V8RecursionScope.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/MainThreadDebugger.h"
#include "public/platform/Platform.h"
#include "wtf/MainThread.h"

namespace blink {

static V8PerIsolateData* mainThreadPerIsolateData = 0;

#if ENABLE(ASSERT)
static void assertV8RecursionScope()
{
    ASSERT(V8RecursionScope::properlyUsed(v8::Isolate::GetCurrent()));
}

static bool runningUnitTest()
{
    return Platform::current()->unitTestSupport();
}
#endif

static void useCounterCallback(v8::Isolate* isolate, v8::Isolate::UseCounterFeature feature)
{
    UseCounter::Feature blinkFeature;
    bool deprecated = false;
    switch (feature) {
    case v8::Isolate::kUseAsm:
        blinkFeature = UseCounter::UseAsm;
        break;
    case v8::Isolate::kBreakIterator:
        blinkFeature = UseCounter::BreakIterator;
        break;
    case v8::Isolate::kLegacyConst:
        blinkFeature = UseCounter::LegacyConst;
        break;
    case v8::Isolate::kObjectObserve:
        blinkFeature = UseCounter::ObjectObserve;
        deprecated = true;
        break;
    case v8::Isolate::kSloppyMode:
        blinkFeature = UseCounter::V8SloppyMode;
        break;
    case v8::Isolate::kStrictMode:
        blinkFeature = UseCounter::V8StrictMode;
        break;
    case v8::Isolate::kStrongMode:
        blinkFeature = UseCounter::V8StrongMode;
        break;
    case v8::Isolate::kRegExpPrototypeStickyGetter:
        blinkFeature = UseCounter::V8RegExpPrototypeStickyGetter;
        break;
    case v8::Isolate::kRegExpPrototypeToString:
        blinkFeature = UseCounter::V8RegExpPrototypeToString;
        break;
    case v8::Isolate::kRegExpPrototypeUnicodeGetter:
        blinkFeature = UseCounter::V8RegExpPrototypeUnicodeGetter;
        break;
    case v8::Isolate::kIntlV8Parse:
        blinkFeature = UseCounter::V8IntlV8Parse;
        break;
    case v8::Isolate::kIntlPattern:
        blinkFeature = UseCounter::V8IntlPattern;
        break;
    case v8::Isolate::kIntlResolved:
        blinkFeature = UseCounter::V8IntlResolved;
        break;
    case v8::Isolate::kPromiseChain:
        blinkFeature = UseCounter::V8PromiseChain;
        break;
    case v8::Isolate::kPromiseAccept:
        blinkFeature = UseCounter::V8PromiseAccept;
        break;
    case v8::Isolate::kPromiseDefer:
        blinkFeature = UseCounter::V8PromiseDefer;
        break;
    default:
        // This can happen if V8 has added counters that this version of Blink
        // does not know about. It's harmless.
        return;
    }
    if (deprecated)
        UseCounter::countDeprecation(currentExecutionContext(isolate), blinkFeature);
    else
        UseCounter::count(currentExecutionContext(isolate), blinkFeature);
}

V8PerIsolateData::V8PerIsolateData()
    : m_destructionPending(false)
    , m_isolateHolder(adoptPtr(new gin::IsolateHolder()))
    , m_stringCache(adoptPtr(new StringCache(isolate())))
    , m_hiddenValue(V8HiddenValue::create())
    , m_constructorMode(ConstructorMode::CreateNewObject)
    , m_recursionLevel(0)
    , m_isHandlingRecursionLevelError(false)
    , m_isReportingException(false)
#if ENABLE(ASSERT)
    , m_internalScriptRecursionLevel(0)
#endif
    , m_performingMicrotaskCheckpoint(false)
    , m_scriptDebugger(nullptr)
{
    // FIXME: Remove once all v8::Isolate::GetCurrent() calls are gone.
    isolate()->Enter();
#if ENABLE(ASSERT)
    if (!runningUnitTest())
        isolate()->AddCallCompletedCallback(&assertV8RecursionScope);
#endif
    if (isMainThread())
        mainThreadPerIsolateData = this;
    isolate()->SetUseCounterCallback(&useCounterCallback);
}

V8PerIsolateData::~V8PerIsolateData()
{
}

v8::Isolate* V8PerIsolateData::mainThreadIsolate()
{
    ASSERT(isMainThread());
    ASSERT(mainThreadPerIsolateData);
    return mainThreadPerIsolateData->isolate();
}

v8::Isolate* V8PerIsolateData::initialize()
{
    V8PerIsolateData* data = new V8PerIsolateData();
    v8::Isolate* isolate = data->isolate();
    isolate->SetData(gin::kEmbedderBlink, data);
    return isolate;
}

void V8PerIsolateData::enableIdleTasks(v8::Isolate* isolate, PassOwnPtr<gin::V8IdleTaskRunner> taskRunner)
{
    from(isolate)->m_isolateHolder->EnableIdleTasks(scoped_ptr<gin::V8IdleTaskRunner>(taskRunner.leakPtr()));
}

v8::Persistent<v8::Value>& V8PerIsolateData::ensureLiveRoot()
{
    if (m_liveRoot.isEmpty())
        m_liveRoot.set(isolate(), v8::Null(isolate()));
    return m_liveRoot.getUnsafe();
}

// willBeDestroyed() clear things that should be cleared before
// ThreadState::detach() gets called.
void V8PerIsolateData::willBeDestroyed(v8::Isolate* isolate)
{
    V8PerIsolateData* data = from(isolate);

    ASSERT(!data->m_destructionPending);
    data->m_destructionPending = true;

    data->m_scriptDebugger.clear();
    // Clear any data that may have handles into the heap,
    // prior to calling ThreadState::detach().
    data->clearEndOfScopeTasks();
}

// destroy() clear things that should be cleared after ThreadState::detach()
// gets called but before the Isolate exits.
void V8PerIsolateData::destroy(v8::Isolate* isolate)
{
#if ENABLE(ASSERT)
    if (!runningUnitTest())
        isolate->RemoveCallCompletedCallback(&assertV8RecursionScope);
#endif
    V8PerIsolateData* data = from(isolate);

    // Clear everything before exiting the Isolate.
    if (data->m_scriptRegexpScriptState)
        data->m_scriptRegexpScriptState->disposePerContextData();
    data->m_liveRoot.clear();
    data->m_hiddenValue.clear();
    data->m_stringCache->dispose();
    data->m_stringCache.clear();
    data->m_domTemplateMapForNonMainWorld.clear();
    data->m_domTemplateMapForMainWorld.clear();
    if (isMainThread())
        mainThreadPerIsolateData = 0;

    // FIXME: Remove once all v8::Isolate::GetCurrent() calls are gone.
    isolate->Exit();
    delete data;
}

V8PerIsolateData::DOMTemplateMap& V8PerIsolateData::currentDOMTemplateMap()
{
    if (DOMWrapperWorld::current(isolate()).isMainWorld())
        return m_domTemplateMapForMainWorld;
    return m_domTemplateMapForNonMainWorld;
}

v8::Local<v8::FunctionTemplate> V8PerIsolateData::domTemplate(const void* domTemplateKey, v8::FunctionCallback callback, v8::Local<v8::Value> data, v8::Local<v8::Signature> signature, int length)
{
    DOMTemplateMap& domTemplateMap = currentDOMTemplateMap();
    DOMTemplateMap::iterator result = domTemplateMap.find(domTemplateKey);
    if (result != domTemplateMap.end())
        return result->value.Get(isolate());

    v8::Local<v8::FunctionTemplate> templ = v8::FunctionTemplate::New(isolate(), callback, data, signature, length);
    domTemplateMap.add(domTemplateKey, v8::Eternal<v8::FunctionTemplate>(isolate(), templ));
    return templ;
}

v8::Local<v8::FunctionTemplate> V8PerIsolateData::existingDOMTemplate(const void* domTemplateKey)
{
    DOMTemplateMap& domTemplateMap = currentDOMTemplateMap();
    DOMTemplateMap::iterator result = domTemplateMap.find(domTemplateKey);
    if (result != domTemplateMap.end())
        return result->value.Get(isolate());
    return v8::Local<v8::FunctionTemplate>();
}

void V8PerIsolateData::setDOMTemplate(const void* domTemplateKey, v8::Local<v8::FunctionTemplate> templ)
{
    currentDOMTemplateMap().add(domTemplateKey, v8::Eternal<v8::FunctionTemplate>(isolate(), v8::Local<v8::FunctionTemplate>(templ)));
}

v8::Local<v8::Context> V8PerIsolateData::ensureScriptRegexpContext()
{
    if (!m_scriptRegexpScriptState) {
        v8::Local<v8::Context> context(v8::Context::New(isolate()));
        m_scriptRegexpScriptState = ScriptState::create(context, DOMWrapperWorld::create(isolate()));
    }
    return m_scriptRegexpScriptState->context();
}

void V8PerIsolateData::clearScriptRegexpContext()
{
    if (m_scriptRegexpScriptState)
        m_scriptRegexpScriptState->disposePerContextData();
    m_scriptRegexpScriptState.clear();
}

bool V8PerIsolateData::hasInstance(const WrapperTypeInfo* untrustedWrapperTypeInfo, v8::Local<v8::Value> value)
{
    return hasInstance(untrustedWrapperTypeInfo, value, m_domTemplateMapForMainWorld)
        || hasInstance(untrustedWrapperTypeInfo, value, m_domTemplateMapForNonMainWorld);
}

bool V8PerIsolateData::hasInstance(const WrapperTypeInfo* untrustedWrapperTypeInfo, v8::Local<v8::Value> value, DOMTemplateMap& domTemplateMap)
{
    DOMTemplateMap::iterator result = domTemplateMap.find(untrustedWrapperTypeInfo);
    if (result == domTemplateMap.end())
        return false;
    v8::Local<v8::FunctionTemplate> templ = result->value.Get(isolate());
    return templ->HasInstance(value);
}

v8::Local<v8::Object> V8PerIsolateData::findInstanceInPrototypeChain(const WrapperTypeInfo* info, v8::Local<v8::Value> value)
{
    v8::Local<v8::Object> wrapper = findInstanceInPrototypeChain(info, value, m_domTemplateMapForMainWorld);
    if (!wrapper.IsEmpty())
        return wrapper;
    return findInstanceInPrototypeChain(info, value, m_domTemplateMapForNonMainWorld);
}

v8::Local<v8::Object> V8PerIsolateData::findInstanceInPrototypeChain(const WrapperTypeInfo* info, v8::Local<v8::Value> value, DOMTemplateMap& domTemplateMap)
{
    if (value.IsEmpty() || !value->IsObject())
        return v8::Local<v8::Object>();
    DOMTemplateMap::iterator result = domTemplateMap.find(info);
    if (result == domTemplateMap.end())
        return v8::Local<v8::Object>();
    v8::Local<v8::FunctionTemplate> templ = result->value.Get(isolate());
    return v8::Local<v8::Object>::Cast(value)->FindInstanceInPrototypeChain(templ);
}

void V8PerIsolateData::addEndOfScopeTask(PassOwnPtr<EndOfScopeTask> task)
{
    m_endOfScopeTasks.append(task);
}

void V8PerIsolateData::runEndOfScopeTasks()
{
    Vector<OwnPtr<EndOfScopeTask>> tasks;
    tasks.swap(m_endOfScopeTasks);
    for (const auto& task : tasks)
        task->run();
    ASSERT(m_endOfScopeTasks.isEmpty());
}

void V8PerIsolateData::clearEndOfScopeTasks()
{
    m_endOfScopeTasks.clear();
}

void V8PerIsolateData::setScriptDebugger(PassOwnPtr<MainThreadDebugger> debugger)
{
    ASSERT(!m_scriptDebugger);
    m_scriptDebugger = std::move(debugger);
}

} // namespace blink
