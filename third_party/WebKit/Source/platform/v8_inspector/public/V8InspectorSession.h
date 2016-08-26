// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8InspectorSession_h
#define V8InspectorSession_h

#include "platform/v8_inspector/public/StringBuffer.h"
#include "platform/v8_inspector/public/StringView.h"
#include "platform/v8_inspector/public/protocol/Debugger.h"
#include "platform/v8_inspector/public/protocol/Runtime.h"
#include "platform/v8_inspector/public/protocol/Schema.h"

#include <v8.h>

namespace v8_inspector {

class PLATFORM_EXPORT V8InspectorSession {
public:
    virtual ~V8InspectorSession() { }

    // Cross-context inspectable values (DOM nodes in different worlds, etc.).
    class Inspectable {
    public:
        virtual v8::Local<v8::Value> get(v8::Local<v8::Context>) = 0;
        virtual ~Inspectable() { }
    };
    virtual void addInspectedObject(std::unique_ptr<Inspectable>) = 0;

    // Dispatching protocol messages.
    static bool canDispatchMethod(const StringView& method);
    virtual void dispatchProtocolMessage(const StringView& message) = 0;
    virtual std::unique_ptr<StringBuffer> stateJSON() = 0;
    virtual std::unique_ptr<blink::protocol::Array<blink::protocol::Schema::API::Domain>> supportedDomains() = 0;

    // Debugger actions.
    virtual void schedulePauseOnNextStatement(const StringView& breakReason, const StringView& breakDetails) = 0;
    virtual void cancelPauseOnNextStatement() = 0;
    virtual void breakProgram(const StringView& breakReason, const StringView& breakDetails) = 0;
    virtual void setSkipAllPauses(bool) = 0;
    virtual void resume() = 0;
    virtual void stepOver() = 0;
    virtual std::unique_ptr<blink::protocol::Array<blink::protocol::Debugger::API::SearchMatch>> searchInTextByLines(const StringView& text, const StringView& query, bool caseSensitive, bool isRegex) = 0;

    // Remote objects.
    virtual std::unique_ptr<blink::protocol::Runtime::API::RemoteObject> wrapObject(v8::Local<v8::Context>, v8::Local<v8::Value>, const StringView& groupName) = 0;
    virtual bool unwrapObject(ErrorString*, const StringView& objectId, v8::Local<v8::Value>*, v8::Local<v8::Context>*, std::unique_ptr<StringBuffer>* objectGroup) = 0;
    virtual void releaseObjectGroup(const StringView&) = 0;
};

} // namespace v8_inspector

#endif // V8InspectorSession_h
