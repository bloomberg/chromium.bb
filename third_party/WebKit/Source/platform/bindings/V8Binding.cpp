/*
 * Copyright (C) 2006, 2007, 2008, 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/bindings/V8Binding.h"

#include "platform/bindings/V8BindingMacros.h"

namespace blink {

v8::Local<v8::Function> GetBoundFunction(v8::Local<v8::Function> function) {
  v8::Local<v8::Value> bound_function = function->GetBoundFunction();
  return bound_function->IsFunction()
             ? v8::Local<v8::Function>::Cast(bound_function)
             : function;
}

bool AddHiddenValueToArray(v8::Isolate* isolate,
                           v8::Local<v8::Object> object,
                           v8::Local<v8::Value> value,
                           int array_index) {
  DCHECK(!value.IsEmpty());
  v8::Local<v8::Value> array_value = object->GetInternalField(array_index);
  if (array_value->IsNull() || array_value->IsUndefined()) {
    array_value = v8::Array::New(isolate);
    object->SetInternalField(array_index, array_value);
  }

  v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(array_value);
  return V8CallBoolean(array->CreateDataProperty(isolate->GetCurrentContext(),
                                                 array->Length(), value));
}

void RemoveHiddenValueFromArray(v8::Isolate* isolate,
                                v8::Local<v8::Object> object,
                                v8::Local<v8::Value> value,
                                int array_index) {
  v8::Local<v8::Value> array_value = object->GetInternalField(array_index);
  if (!array_value->IsArray())
    return;
  v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(array_value);
  for (int i = array->Length() - 1; i >= 0; --i) {
    v8::Local<v8::Value> item;
    if (!array->Get(isolate->GetCurrentContext(), i).ToLocal(&item))
      return;
    if (item->StrictEquals(value)) {
      array->Delete(isolate->GetCurrentContext(), i).ToChecked();
      return;
    }
  }
}

v8::Local<v8::Value> FreezeV8Object(v8::Local<v8::Value> value,
                                    v8::Isolate* isolate) {
  value.As<v8::Object>()
      ->SetIntegrityLevel(isolate->GetCurrentContext(),
                          v8::IntegrityLevel::kFrozen)
      .ToChecked();
  return value;
}

}  // namespace blink
