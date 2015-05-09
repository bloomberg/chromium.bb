// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PerIsolateDebuggerClient_h
#define PerIsolateDebuggerClient_h

#include "bindings/core/v8/ScriptDebugServer.h"

namespace blink {

class PerIsolateDebuggerClient : public ScriptDebugServer::Client {
    WTF_MAKE_NONCOPYABLE(PerIsolateDebuggerClient);
public:
    PerIsolateDebuggerClient(v8::Isolate*, PassOwnPtr<ScriptDebugServer>);
    ~PerIsolateDebuggerClient() override;
    v8::Local<v8::Object> compileDebuggerScript() override;
    ScriptDebugServer* scriptDebugServer() const { return m_scriptDebugServer.get(); }
private:
    v8::Isolate* m_isolate;
    OwnPtr<ScriptDebugServer> m_scriptDebugServer;
};

} // namespace blink


#endif // !defined(PerIsolateDebuggerClient_h)
