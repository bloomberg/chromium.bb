// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptDebuggerBase_h
#define ScriptDebuggerBase_h

#include "core/CoreExport.h"
#include "core/inspector/v8/V8Debugger.h"
#include "core/inspector/v8/V8DebuggerClient.h"

namespace blink {

class CORE_EXPORT ScriptDebuggerBase : public V8DebuggerClient {
    WTF_MAKE_NONCOPYABLE(ScriptDebuggerBase);
public:
    ScriptDebuggerBase(v8::Isolate*);
    ~ScriptDebuggerBase() override;
    v8::Local<v8::Object> compileDebuggerScript() override;
    V8Debugger* debugger() const { return m_debugger.get(); }

private:
    v8::Isolate* m_isolate;
    OwnPtr<V8Debugger> m_debugger;
};

} // namespace blink


#endif // !defined(ScriptDebuggerBase_h)
