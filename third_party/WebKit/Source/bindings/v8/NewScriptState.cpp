// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/v8/NewScriptState.h"

#include "bindings/v8/V8Binding.h"
#include "core/frame/LocalFrame.h"

namespace WebCore {

PassRefPtr<NewScriptState> NewScriptState::create(v8::Handle<v8::Context> context, PassRefPtr<DOMWrapperWorld> world)
{
    RefPtr<NewScriptState> scriptState = adoptRef(new NewScriptState(context, world));
    // This ref() is for keeping this NewScriptState alive as long as the v8::Context is alive.
    // This is deref()ed in the weak callback of the v8::Context.
    scriptState->ref();
    return scriptState;
}

static void weakCallback(const v8::WeakCallbackData<v8::Context, NewScriptState>& data)
{
    data.GetValue()->SetAlignedPointerInEmbedderData(v8ContextPerContextDataIndex, 0);
    data.GetParameter()->clearContext();
    data.GetParameter()->deref();
}

NewScriptState::NewScriptState(v8::Handle<v8::Context> context, PassRefPtr<DOMWrapperWorld> world)
    : m_isolate(context->GetIsolate())
    , m_context(m_isolate, context)
    , m_world(world)
    , m_perContextData(V8PerContextData::create(context, m_world))
{
    ASSERT(m_world);
    m_context.setWeak(this, &weakCallback);
    context->SetAlignedPointerInEmbedderData(v8ContextPerContextDataIndex, this);
}

NewScriptState::~NewScriptState()
{
    ASSERT(!m_perContextData);
    ASSERT(m_context.isEmpty());
}

ExecutionContext* NewScriptState::executionContext() const
{
    return toExecutionContext(context());
}

NewScriptState* NewScriptState::forMainWorld(LocalFrame* frame)
{
    v8::Isolate* isolate = toIsolate(frame);
    v8::HandleScope handleScope(isolate);
    return NewScriptState::from(toV8Context(isolate, frame, DOMWrapperWorld::mainWorld()));
}

}
