// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptDebuggerBase_h
#define ScriptDebuggerBase_h

#include "bindings/core/v8/ScriptDebugServer.h"
#include "platform/heap/Handle.h"

namespace blink {

class ScriptDebuggerBase : public ScriptDebugServer::Client {
    WTF_MAKE_NONCOPYABLE(ScriptDebuggerBase);
public:
    ScriptDebuggerBase(v8::Isolate*, PassOwnPtrWillBeRawPtr<ScriptDebugServer>);
    ~ScriptDebuggerBase() override;
    v8::Local<v8::Object> compileDebuggerScript() override;
    ScriptDebugServer* scriptDebugServer() const { return m_scriptDebugServer.get(); }

    DECLARE_VIRTUAL_TRACE();

private:
    v8::Isolate* m_isolate;
    OwnPtrWillBeMember<ScriptDebugServer> m_scriptDebugServer;
};

} // namespace blink


#endif // !defined(ScriptDebuggerBase_h)
