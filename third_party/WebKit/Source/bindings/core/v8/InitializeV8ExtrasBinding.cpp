// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/InitializeV8ExtrasBinding.h"

#include <algorithm>
#include <iterator>

#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/ToV8ForCore.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/UseCounter.h"
#include "core/frame/WebFeature.h"
#include "platform/bindings/ToV8.h"
#include "platform/bindings/V8Binding.h"
#include "platform/bindings/V8ThrowException.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

// This macro avoids duplicating the name and hence prevents typos.
#define WEB_FEATURE_ID_NAME_LOOKUP_ENTRY(id) \
  { #id, WebFeature::k##id }

struct WebFeatureIdNameLookupEntry {
  const char* const name;
  const WebFeature id;
};

// TODO(ricea): Replace with a more efficient data structure if the
// number of entries increases.
constexpr WebFeatureIdNameLookupEntry web_feature_id_name_lookup_table[] = {
    WEB_FEATURE_ID_NAME_LOOKUP_ENTRY(ReadableStreamConstructor),
    WEB_FEATURE_ID_NAME_LOOKUP_ENTRY(WritableStreamConstructor),
    WEB_FEATURE_ID_NAME_LOOKUP_ENTRY(TransformStreamConstructor),
};

#undef WEB_FEATURE_ID_NAME_LOOKUP_ENTRY

class CountUseForBindings : public ScriptFunction {
 public:
  static v8::Local<v8::Function> CreateFunction(ScriptState* script_state) {
    auto* self = new CountUseForBindings(script_state);
    return self->BindToV8Function();
  }

 private:
  explicit CountUseForBindings(ScriptState* script_state)
      : ScriptFunction(script_state) {}

  ScriptValue Call(ScriptValue value) override {
    String string_id;
    if (!value.ToString(string_id)) {
      V8ThrowException::ThrowTypeError(value.GetIsolate(),
                                       "countUse requires a string argument");
      return ScriptValue();
    }

    const auto it =
        std::find_if(std::begin(web_feature_id_name_lookup_table),
                     std::end(web_feature_id_name_lookup_table),
                     [&string_id](const WebFeatureIdNameLookupEntry& entry) {
                       return string_id == entry.name;
                     });

    if (it == std::end(web_feature_id_name_lookup_table)) {
      V8ThrowException::ThrowTypeError(value.GetIsolate(),
                                       "unknown use counter");
      return ScriptValue();
    }

    UseCounter::Count(ExecutionContext::From(GetScriptState()), it->id);

    return ScriptValue::From(GetScriptState(), ToV8UndefinedGenerator());
  }
};

}  // namespace

void InitializeV8ExtrasBinding(ScriptState* script_state) {
  v8::Local<v8::Object> binding =
      script_state->GetContext()->GetExtrasBindingObject();
  v8::Local<v8::Function> fn =
      CountUseForBindings::CreateFunction(script_state);
  v8::Local<v8::String> name =
      V8AtomicString(script_state->GetIsolate(), "countUse");
  binding->Set(script_state->GetContext(), name, fn).FromJust();
}

}  // namespace blink
