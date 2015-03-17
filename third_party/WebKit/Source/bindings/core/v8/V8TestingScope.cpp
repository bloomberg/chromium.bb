// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/core/v8/V8TestingScope.h"

#include "bindings/core/v8/DOMWrapperWorld.h"

namespace blink {

V8TestingScope::V8TestingScope(v8::Isolate* isolate)
    : m_handleScope(isolate)
    , m_contextScope(v8::Context::New(isolate))
    // We reuse the main world since the main world is guaranteed to be registered to ScriptController.
    , m_scriptState(ScriptStateForTesting::create(isolate->GetCurrentContext(), &DOMWrapperWorld::mainWorld()))
{
}

V8TestingScope::~V8TestingScope()
{
    m_scriptState->disposePerContextData();
}

ScriptState* V8TestingScope::scriptState() const
{
    return m_scriptState.get();
}

v8::Isolate* V8TestingScope::isolate() const
{
    return m_scriptState->isolate();
}

} // namespace blink
