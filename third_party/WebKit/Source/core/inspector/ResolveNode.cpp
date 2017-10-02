// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/ResolveNode.h"

#include "bindings/core/v8/BindingSecurity.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/Document.h"
#include "core/dom/Node.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/V8InspectorString.h"

namespace blink {

v8::Local<v8::Value> NodeV8Value(v8::Local<v8::Context> context, Node* node) {
  v8::Isolate* isolate = context->GetIsolate();
  if (!node || !BindingSecurity::ShouldAllowAccessTo(
                   CurrentDOMWindow(isolate), node,
                   BindingSecurity::ErrorReportOption::kDoNotReport))
    return v8::Null(isolate);
  return ToV8(node, context->Global(), isolate);
}

std::unique_ptr<v8_inspector::protocol::Runtime::API::RemoteObject> ResolveNode(
    v8_inspector::V8InspectorSession* v8_session,
    Node* node,
    const String& object_group) {
  if (!node)
    return nullptr;

  Document* document =
      node->IsDocumentNode() ? &node->GetDocument() : node->ownerDocument();
  LocalFrame* frame = document ? document->GetFrame() : nullptr;
  if (!frame)
    return nullptr;

  ScriptState* script_state = ToScriptStateForMainWorld(frame);
  if (!script_state)
    return nullptr;

  ScriptState::Scope scope(script_state);
  return v8_session->wrapObject(script_state->GetContext(),
                                NodeV8Value(script_state->GetContext(), node),
                                ToV8InspectorStringView(object_group));
}

std::unique_ptr<v8_inspector::protocol::Runtime::API::RemoteObject>
NullRemoteObject(v8_inspector::V8InspectorSession* v8_session,
                 LocalFrame* frame,
                 const String& object_group) {
  if (!frame)
    return nullptr;

  ScriptState* script_state = ToScriptStateForMainWorld(frame);
  if (!script_state)
    return nullptr;

  ScriptState::Scope scope(script_state);
  return v8_session->wrapObject(
      script_state->GetContext(),
      NodeV8Value(script_state->GetContext(), nullptr),
      ToV8InspectorStringView(object_group));
}

}  // namespace blink
