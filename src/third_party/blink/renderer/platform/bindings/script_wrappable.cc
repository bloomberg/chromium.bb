// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/bindings/binding_security_for_platform.h"

namespace blink {

struct SameSizeAsScriptWrappable {
  virtual ~SameSizeAsScriptWrappable() = default;
  v8::Persistent<v8::Object> main_world_wrapper_;
};

static_assert(sizeof(ScriptWrappable) <= sizeof(SameSizeAsScriptWrappable),
              "ScriptWrappable should stay small");

v8::Local<v8::Object> ScriptWrappable::Wrap(
    v8::Isolate* isolate,
    v8::Local<v8::Object> creation_context) {
  const WrapperTypeInfo* wrapper_type_info = this->GetWrapperTypeInfo();

  if (// This throws if the current context does not have a 'ScriptState'
      // associated with it:
      !ScriptState::AccessCheck(isolate->GetCurrentContext()) ||
       // If the current context is the same as the creation context, assume
       // it's valid, otherwise call out to verify:
      (isolate->GetCurrentContext() != creation_context->CreationContext() &&
       // This basically fulfills the TODO in 'V8DOMWrapper::CreateWrapper()'
       !BindingSecurityForPlatform::ShouldAllowWrapperCreationOrThrowException(
           isolate->GetCurrentContext(), creation_context->CreationContext(), wrapper_type_info))) {
      const String& message =
        "DOM access from invalid context";
      V8ThrowException::ThrowAccessError(
        isolate, message);
      return v8::Local<v8::Object>();
  }

  DCHECK(!DOMDataStore::ContainsWrapper(this, isolate));

  v8::Local<v8::Object> wrapper =
      V8DOMWrapper::CreateWrapper(isolate, creation_context, wrapper_type_info);
  DCHECK(!wrapper.IsEmpty());
  return AssociateWithWrapper(isolate, wrapper_type_info, wrapper);
}

v8::Local<v8::Object> ScriptWrappable::AssociateWithWrapper(
    v8::Isolate* isolate,
    const WrapperTypeInfo* wrapper_type_info,
    v8::Local<v8::Object> wrapper) {
  return V8DOMWrapper::AssociateObjectWithWrapper(isolate, this,
                                                  wrapper_type_info, wrapper);
}

void ScriptWrappable::Trace(Visitor* visitor) {
  visitor->Trace(main_world_wrapper_);
}

const char* ScriptWrappable::NameInHeapSnapshot() const {
  return GetWrapperTypeInfo()->interface_name;
}

}  // namespace blink
