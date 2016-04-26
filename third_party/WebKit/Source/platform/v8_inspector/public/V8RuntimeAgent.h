// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8RuntimeAgent_h
#define V8RuntimeAgent_h

#include "platform/PlatformExport.h"
#include "platform/inspector_protocol/Dispatcher.h"
#include "platform/v8_inspector/public/V8ContextInfo.h"
#include "platform/v8_inspector/public/V8Debugger.h"

#include <v8.h>

namespace blink {

class InjectedScriptManager;

class PLATFORM_EXPORT V8RuntimeAgent : public protocol::Backend::Runtime, public V8Debugger::Agent<protocol::Frontend::Runtime> {
public:
    // Cross-context inspectable values (DOM nodes in different worlds, etc.).
    class Inspectable {
    public:
        virtual v8::Local<v8::Value> get(v8::Local<v8::Context>) = 0;
        virtual ~Inspectable() { }
    };

    virtual ~V8RuntimeAgent() { }

    // Embedder API.
    virtual PassOwnPtr<protocol::Runtime::RemoteObject> wrapObject(v8::Local<v8::Context>, v8::Local<v8::Value>, const String16& groupName, bool generatePreview = false) = 0;
    // FIXME: remove when InspectorConsoleAgent moves into V8 inspector.
    virtual PassOwnPtr<protocol::Runtime::RemoteObject> wrapTable(v8::Local<v8::Context>, v8::Local<v8::Value> table, v8::Local<v8::Value> columns) = 0;
    virtual v8::Local<v8::Value> findObject(ErrorString*, const String16& objectId, v8::Local<v8::Context>* = nullptr, String16* objectGroup = nullptr) = 0;
    virtual void disposeObjectGroup(const String16&) = 0;
    virtual void addInspectedObject(PassOwnPtr<Inspectable>) = 0;
};

} // namespace blink

#endif // V8RuntimeAgent_h
