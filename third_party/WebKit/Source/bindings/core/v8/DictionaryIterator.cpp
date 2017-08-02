// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/DictionaryIterator.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8StringResource.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

DictionaryIterator::DictionaryIterator(v8::Local<v8::Object> iterator,
                                       v8::Isolate* isolate)
    : isolate_(isolate),
      iterator_(iterator),
      next_key_(V8AtomicString(isolate, "next")),
      done_key_(V8AtomicString(isolate, "done")),
      value_key_(V8AtomicString(isolate, "value")),
      done_(false) {
  DCHECK(!iterator.IsEmpty());
}

bool DictionaryIterator::Next(ExecutionContext* execution_context,
                              ExceptionState& exception_state) {
  DCHECK(!IsNull());

  v8::TryCatch try_catch(isolate_);
  v8::Local<v8::Context> context = isolate_->GetCurrentContext();

  v8::Local<v8::Value> next;
  if (!iterator_->Get(context, next_key_).ToLocal(&next)) {
    CHECK(!try_catch.Exception().IsEmpty());
    exception_state.RethrowV8Exception(try_catch.Exception());
    done_ = true;
    return false;
  }
  if (!next->IsFunction()) {
    exception_state.ThrowTypeError("Expected next() function on iterator.");
    done_ = true;
    return false;
  }

  v8::Local<v8::Value> result;
  if (!V8ScriptRunner::CallFunction(v8::Local<v8::Function>::Cast(next),
                                    execution_context, iterator_, 0, nullptr,
                                    isolate_)
           .ToLocal(&result)) {
    CHECK(!try_catch.Exception().IsEmpty());
    exception_state.RethrowV8Exception(try_catch.Exception());
    done_ = true;
    return false;
  }
  if (!result->IsObject()) {
    exception_state.ThrowTypeError(
        "Expected iterator.next() to return an Object.");
    done_ = true;
    return false;
  }
  v8::Local<v8::Object> result_object = v8::Local<v8::Object>::Cast(result);

  value_ = result_object->Get(context, value_key_);
  if (value_.IsEmpty()) {
    CHECK(!try_catch.Exception().IsEmpty());
    exception_state.RethrowV8Exception(try_catch.Exception());
  }

  v8::Local<v8::Value> done;
  v8::Local<v8::Boolean> done_boolean;
  if (!result_object->Get(context, done_key_).ToLocal(&done) ||
      !done->ToBoolean(context).ToLocal(&done_boolean)) {
    CHECK(!try_catch.Exception().IsEmpty());
    exception_state.RethrowV8Exception(try_catch.Exception());
    done_ = true;
    return false;
  }

  done_ = done_boolean->Value();
  return !done_;
}

bool DictionaryIterator::ValueAsDictionary(Dictionary& result,
                                           ExceptionState& exception_state) {
  DCHECK(!IsNull());
  DCHECK(!done_);

  v8::Local<v8::Value> value;
  if (!value_.ToLocal(&value) || !value->IsObject())
    return false;

  result = Dictionary(isolate_, value, exception_state);
  return true;
}

bool DictionaryIterator::ValueAsString(String& result) {
  DCHECK(!IsNull());
  DCHECK(!done_);

  v8::Local<v8::Value> value;
  if (!value_.ToLocal(&value))
    return false;

  V8StringResource<> string_value(value);
  if (!string_value.Prepare())
    return false;
  result = string_value;
  return true;
}

}  // namespace blink
