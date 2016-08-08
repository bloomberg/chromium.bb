// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SimpleInspector_h
#define SimpleInspector_h

#include "platform/inspector_protocol/Platform.h"
#include "platform/v8_inspector/public/V8InspectorClient.h"

#include <v8.h>

namespace blink {

namespace protocol {
class Dispatcher;
class Frontend;
class FrontendChannel;
}

class V8Inspector;
class V8InspectorSession;

class SimpleInspector : public V8InspectorClient {
public:
    SimpleInspector(v8::Isolate*, v8::Local<v8::Context>);
    ~SimpleInspector() override;

    // Transport interface.
    void connectFrontend(protocol::FrontendChannel*);
    void disconnectFrontend();
    void dispatchMessageFromFrontend(const String16& message);
    void notifyContextDestroyed();

private:
    v8::Local<v8::Context> ensureDefaultContextInGroup(int contextGroupId) override;

    std::unique_ptr<V8Inspector> m_inspector;
    std::unique_ptr<V8InspectorSession> m_session;
    v8::Local<v8::Context> m_context;
};

}

#endif // SimpleInspector_h
