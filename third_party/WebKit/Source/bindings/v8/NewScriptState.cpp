// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/v8/NewScriptState.h"

#include "bindings/v8/V8Binding.h"

namespace WebCore {

void NewScriptState::install(v8::Handle<v8::Context> context, PassRefPtr<DOMWrapperWorld> world)
{
    RefPtr<NewScriptState> scriptState = adoptRef(new NewScriptState(context, world));
    scriptState->ref();
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
    , m_perContextData(0)
    , m_world(world)
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

}
