// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "platform/v8_inspector/public/SimpleInspector.h"

#include "platform/inspector_protocol/DispatcherBase.h"
#include "platform/v8_inspector/public/V8Inspector.h"
#include "platform/v8_inspector/public/V8InspectorSession.h"

namespace blink {

SimpleInspector::SimpleInspector(v8::Isolate* isolate, v8::Local<v8::Context> context)
    : m_context(context)
{
    m_inspector = V8Inspector::create(isolate, this);
    m_inspector->contextCreated(V8ContextInfo(context, 1, "NodeJS Main Context"));
}

SimpleInspector::~SimpleInspector()
{
    disconnectFrontend();
}

void SimpleInspector::connectFrontend(protocol::FrontendChannel* channel)
{
    m_session = m_inspector->connect(1, channel, nullptr);
}

void SimpleInspector::disconnectFrontend()
{
    m_session.reset();
}

void SimpleInspector::dispatchMessageFromFrontend(const String16& message)
{
    if (m_session)
        m_session->dispatchProtocolMessage(message);
}

v8::Local<v8::Context> SimpleInspector::ensureDefaultContextInGroup(int contextGroupId)
{
    return m_context;
}

void SimpleInspector::notifyContextDestroyed()
{
    m_inspector->contextDestroyed(m_context);
}

} // namespace blink
