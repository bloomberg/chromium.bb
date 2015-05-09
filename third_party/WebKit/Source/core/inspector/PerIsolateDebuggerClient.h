// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PerIsolateDebuggerClient_h
#define PerIsolateDebuggerClient_h

#include "bindings/core/v8/ScriptDebugServer.h"
#include "platform/heap/Handle.h"

namespace blink {

class PerIsolateDebuggerClient : public ScriptDebugServer::Client {
    WTF_MAKE_NONCOPYABLE(PerIsolateDebuggerClient);
public:
    PerIsolateDebuggerClient(v8::Isolate*, PassOwnPtrWillBeRawPtr<ScriptDebugServer>);
    ~PerIsolateDebuggerClient() override;
    v8::Local<v8::Object> compileDebuggerScript() override;
    ScriptDebugServer* scriptDebugServer() const { return m_scriptDebugServer.get(); }

    DECLARE_VIRTUAL_TRACE();

private:
    v8::Isolate* m_isolate;
    OwnPtrWillBeMember<ScriptDebugServer> m_scriptDebugServer;
};

} // namespace blink


#endif // !defined(PerIsolateDebuggerClient_h)
