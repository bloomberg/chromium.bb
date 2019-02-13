// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_VALUE_OR_SCRIPT_WRAPPABLE_ADAPTER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_VALUE_OR_SCRIPT_WRAPPABLE_ADAPTER_H_

#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "v8/include/v8.h"

namespace blink {

class ScriptState;
class ScriptWrappable;

namespace bindings {

// V8ValueOrScriptWrappableAdapter is an adapter to make bindings functions take
// either of v8::Local<v8::Value> or ScriptWrappable*.
//
// This class is only designed to be used as function argument types in bindings
// layer.  Please refrain from abuse for other purpose and/or outside bindings.
class PLATFORM_EXPORT V8ValueOrScriptWrappableAdapter {
  STACK_ALLOCATED();

 public:
  V8ValueOrScriptWrappableAdapter(std::nullptr_t) {}
  V8ValueOrScriptWrappableAdapter(v8::Local<v8::Value> v8_value)
      : v8_value_(v8_value) {}
  V8ValueOrScriptWrappableAdapter(ScriptWrappable* script_wrappable)
      : script_wrappable_(script_wrappable) {}
  template <typename T>
  V8ValueOrScriptWrappableAdapter(Persistent<T> script_wrappable)
      : script_wrappable_(script_wrappable) {
    static_assert(std::is_base_of<ScriptWrappable, T>::value,
                  "script_wrappable must be a ScriptWrappable");
  }

  v8::Local<v8::Value> V8Value(ScriptState* creation_context) const;

  bool IsEmpty() const { return v8_value_.IsEmpty() && !script_wrappable_; }

 private:
  v8::Local<v8::Value> v8_value_;
  Member<ScriptWrappable> script_wrappable_ = nullptr;
};

}  // namespace bindings

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_VALUE_OR_SCRIPT_WRAPPABLE_ADAPTER_H_
