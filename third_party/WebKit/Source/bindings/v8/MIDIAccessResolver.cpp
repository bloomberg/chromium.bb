// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/v8/MIDIAccessResolver.h"

#include "bindings/v8/ScriptPromiseResolver.h"
#include "bindings/v8/V8Binding.h"
#include <v8.h>

namespace WebCore {

MIDIAccessResolver::MIDIAccessResolver(PassRefPtr<ScriptPromiseResolver> resolver, v8::Isolate* isolate)
    : m_resolver(resolver)
    , m_world(DOMWrapperWorld::current(isolate))
{
}

MIDIAccessResolver::~MIDIAccessResolver()
{
}

void MIDIAccessResolver::resolve(MIDIAccess* access, ExecutionContext* executionContext)
{
    v8::HandleScope handleScope(toIsolate(executionContext));
    v8::Context::Scope contextScope(toV8Context(executionContext, m_world.get()));

    m_resolver->resolve(access, executionContext);
}

void MIDIAccessResolver::reject(DOMError* error, ExecutionContext* executionContext)
{
    v8::HandleScope handleScope(toIsolate(executionContext));
    v8::Context::Scope contextScope(toV8Context(executionContext, m_world.get()));

    m_resolver->reject(error, executionContext);
}

} // namespace WebCore
