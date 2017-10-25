// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8ObjectBuilder_h
#define V8ObjectBuilder_h

#include "bindings/core/v8/ToV8ForCore.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/WTFString.h"
#include "v8/include/v8.h"

namespace blink {

class ScriptState;
class ScriptValue;

class CORE_EXPORT V8ObjectBuilder final {
  STACK_ALLOCATED();

 public:
  explicit V8ObjectBuilder(ScriptState*);

  ScriptState* GetScriptState() const { return script_state_.get(); }

  V8ObjectBuilder& Add(const StringView& name, const V8ObjectBuilder&);

  V8ObjectBuilder& AddNull(const StringView& name);
  V8ObjectBuilder& AddBoolean(const StringView& name, bool value);
  V8ObjectBuilder& AddNumber(const StringView& name, double value);
  V8ObjectBuilder& AddString(const StringView& name, const StringView& value);
  V8ObjectBuilder& AddStringOrNull(const StringView& name,
                                   const StringView& value);

  template <typename T>
  V8ObjectBuilder& Add(const StringView& name, const T& value) {
    AddInternal(name, v8::Local<v8::Value>(
                          ToV8(value, script_state_->GetContext()->Global(),
                               script_state_->GetIsolate())));
    return *this;
  }

  ScriptValue GetScriptValue() const;
  v8::Local<v8::Object> V8Value() const { return object_; }

 private:
  void AddInternal(const StringView& name, v8::Local<v8::Value>);

  scoped_refptr<ScriptState> script_state_;
  v8::Local<v8::Object> object_;
};

}  // namespace blink

#endif  // V8ObjectBuilder_h
