// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/bindings/binding_security_for_platform.h"

namespace blink {

struct SameSizeAsScriptWrappable {
  virtual ~SameSizeAsScriptWrappable() = default;
  v8::Persistent<v8::Object> main_world_wrapper_;
};

ASSERT_SIZE(ScriptWrappable, SameSizeAsScriptWrappable);

v8::MaybeLocal<v8::Value> ScriptWrappable::Wrap(ScriptState* script_state) {
  const WrapperTypeInfo* wrapper_type_info = GetWrapperTypeInfo();
  v8::Isolate* isolate = script_state->GetIsolate();

  if (// This throws if the current context does not have a 'ScriptState'
      // associated with it:
      !ScriptState::AccessCheck(isolate->GetCurrentContext()) ||
       // If the current context is the same as the creation context, assume
       // it's valid, otherwise call out to verify:
      (isolate->GetCurrentContext() != script_state->GetContext() &&
       // This basically fulfills the TODO in 'V8DOMWrapper::CreateWrapper()'
       !BindingSecurityForPlatform::ShouldAllowWrapperCreationOrThrowException(
           isolate->GetCurrentContext(), script_state->GetContext(), wrapper_type_info))) {
      const String& message =
        "DOM access from invalid context";
      V8ThrowException::ThrowAccessError(
        isolate, message);
      return v8::Local<v8::Object>();
  }

  DCHECK(!DOMDataStore::ContainsWrapper(this, script_state->GetIsolate()));

  v8::Local<v8::Object> wrapper;
  if (!V8DOMWrapper::CreateWrapper(script_state, wrapper_type_info)
           .ToLocal(&wrapper)) {
    return v8::MaybeLocal<v8::Value>();
  }
  return AssociateWithWrapper(script_state->GetIsolate(), wrapper_type_info,
                              wrapper);
}

v8::Local<v8::Object> ScriptWrappable::AssociateWithWrapper(
    v8::Isolate* isolate,
    const WrapperTypeInfo* wrapper_type_info,
    v8::Local<v8::Object> wrapper) {
  return V8DOMWrapper::AssociateObjectWithWrapper(isolate, this,
                                                  wrapper_type_info, wrapper);
}

void ScriptWrappable::Trace(Visitor* visitor) const {
  visitor->Trace(main_world_wrapper_);
}

const char* ScriptWrappable::NameInHeapSnapshot() const {
  return GetWrapperTypeInfo()->interface_name;
}

}  // namespace blink
