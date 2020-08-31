// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/font_access/font_manager.h"

#include <algorithm>

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/modules/font_access/font_iterator.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"

namespace blink {

namespace {

void ReturnDataFunction(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValue(info, info.Data());
}

}  // namespace

ScriptValue FontManager::query(ScriptState* script_state) {
  FontCache* font_cache = FontCache::GetFontCache();

  auto* iterator =
      MakeGarbageCollected<FontIterator>(font_cache->EnumerateAvailableFonts());
  auto* isolate = script_state->GetIsolate();
  auto context = script_state->GetContext();

  v8::Local<v8::Object> result = v8::Object::New(isolate);
  if (!result
           ->Set(context, v8::Symbol::GetAsyncIterator(isolate),
                 v8::Function::New(context, &ReturnDataFunction,
                                   ToV8(iterator, script_state))
                     .ToLocalChecked())
           .ToChecked()) {
    return ScriptValue();
  }
  return ScriptValue(script_state->GetIsolate(), result);
}

void FontManager::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
