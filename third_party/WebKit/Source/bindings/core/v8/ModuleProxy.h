// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ModuleProxy_h
#define ModuleProxy_h

#include <v8.h>

namespace blink {

class Event;
class EventTarget;
class ExecutionContext;

// A proxy class to invoke functions implemented in bindings/modules
// from bindings/core.
class ModuleProxy {
public:
    static ModuleProxy& moduleProxy();

    v8::Handle<v8::Object> wrapForEvent(Event*, v8::Handle<v8::Object>, v8::Isolate*);
    void registerWrapForEvent(v8::Handle<v8::Object> (*wrapForEvent)(Event*, v8::Handle<v8::Object>, v8::Isolate*));

    v8::Handle<v8::Value> toV8ForEventTarget(EventTarget*, v8::Handle<v8::Object>, v8::Isolate*);
    void registerToV8ForEventTarget(v8::Handle<v8::Value> (*toV8ForEventTarget)(EventTarget*, v8::Handle<v8::Object>, v8::Isolate*));

    void didLeaveScriptContextForRecursionScope(ExecutionContext&);
    void registerDidLeaveScriptContextForRecursionScope(void (*didLeaveScriptContext)(ExecutionContext&));

private:
    ModuleProxy() : m_wrapForEvent(0) { }

    v8::Handle<v8::Object> (*m_wrapForEvent)(Event*, v8::Handle<v8::Object>, v8::Isolate*);
    v8::Handle<v8::Value> (*m_toV8ForEventTarget)(EventTarget*, v8::Handle<v8::Object>, v8::Isolate*);
    void (*m_didLeaveScriptContextForRecursionScope)(ExecutionContext&);
};

} // namespace blink

#endif // ModuleProxy_h
