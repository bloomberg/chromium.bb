// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8RuntimeAgent_h
#define V8RuntimeAgent_h

#include "core/CoreExport.h"
#include "core/inspector/v8/public/V8Debugger.h"
#include "platform/inspector_protocol/Dispatcher.h"
#include "wtf/Functional.h"

#include <v8.h>

namespace blink {

class InjectedScriptManager;

class CORE_EXPORT V8RuntimeAgent : public protocol::Dispatcher::RuntimeCommandHandler, public V8Debugger::Agent<protocol::Frontend::Runtime> {
public:
    // Cross-context inspectable values (DOM nodes in different worlds, etc.).
    class Inspectable {
    public:
        virtual v8::Local<v8::Value> get(v8::Local<v8::Context>) = 0;
        virtual ~Inspectable() { }
    };

    static PassOwnPtr<V8RuntimeAgent> create(V8Debugger*);
    virtual ~V8RuntimeAgent() { }

    // Embedder notification API.
    virtual void reportExecutionContextCreated(v8::Local<v8::Context>, const String& type, const String& origin, const String& humanReadableName, const String& frameId) = 0;
    virtual void reportExecutionContextDestroyed(v8::Local<v8::Context>) = 0;

    // Embedder API.
    using ClearConsoleCallback = Function<void()>;
    virtual void setClearConsoleCallback(PassOwnPtr<ClearConsoleCallback>) = 0;
    using InspectCallback = Function<void(PassRefPtr<protocol::TypeBuilder::Runtime::RemoteObject>, PassRefPtr<JSONObject>)>;
    virtual void setInspectObjectCallback(PassOwnPtr<InspectCallback>) = 0;
    // FIXME: remove while preserving the default context evaluation.
    virtual int ensureDefaultContextAvailable(v8::Local<v8::Context>) = 0;
    virtual PassRefPtr<protocol::TypeBuilder::Runtime::RemoteObject> wrapObject(v8::Local<v8::Context>, v8::Local<v8::Value>, const String& groupName, bool generatePreview = false) = 0;
    // FIXME: remove when console.table moves into V8 inspector.
    virtual PassRefPtr<protocol::TypeBuilder::Runtime::RemoteObject> wrapTable(v8::Local<v8::Context>, v8::Local<v8::Value> table, v8::Local<v8::Value> columns) = 0;
    virtual v8::Local<v8::Value> findObject(const String& objectId, v8::Local<v8::Context>* = nullptr, String* objectGroup = nullptr) = 0;
    virtual void disposeObjectGroup(const String&) = 0;
    virtual void addInspectedObject(PassOwnPtr<Inspectable>) = 0;
    virtual void clearInspectedObjects() = 0;
};

} // namespace blink

#endif // V8RuntimeAgent_h
