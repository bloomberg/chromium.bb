// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ResolveNode_h
#define ResolveNode_h

#include <memory>

#include "core/inspector/protocol/Runtime.h"
#include "platform/wtf/text/WTFString.h"
#include "v8/include/v8-inspector.h"

namespace blink {

class LocalFrame;
class Node;

v8::Local<v8::Value> NodeV8Value(v8::Local<v8::Context>, Node*);

std::unique_ptr<v8_inspector::protocol::Runtime::API::RemoteObject> ResolveNode(
    v8_inspector::V8InspectorSession*,
    Node*,
    const String& object_group);

std::unique_ptr<v8_inspector::protocol::Runtime::API::RemoteObject>
NullRemoteObject(v8_inspector::V8InspectorSession* v8_session,
                 LocalFrame*,
                 const String& object_group);

}  // namespace blink

#endif  // ResolveNode_h
