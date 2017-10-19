/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "bindings/core/v8/ScriptString.h"

#include "bindings/core/v8/V8BindingForCore.h"

namespace blink {

ScriptString::ScriptString() : isolate_(nullptr) {}

ScriptString::ScriptString(v8::Isolate* isolate, v8::Local<v8::String> string)
    : isolate_(isolate),
      string_(SharedPersistent<v8::String>::Create(string, isolate_)) {}

ScriptString& ScriptString::operator=(const ScriptString& string) {
  if (this != &string) {
    isolate_ = string.isolate_;
    string_ = string.string_;
  }
  return *this;
}

v8::Local<v8::String> ScriptString::V8Value() {
  if (IsEmpty())
    return v8::Local<v8::String>();
  return string_->NewLocal(GetIsolate());
}

ScriptString ScriptString::ConcatenateWith(const String& string) {
  v8::Isolate* non_null_isolate = GetIsolate();
  v8::HandleScope handle_scope(non_null_isolate);
  v8::Local<v8::String> target_string = V8String(non_null_isolate, string);
  if (IsEmpty())
    return ScriptString(non_null_isolate, target_string);
  return ScriptString(non_null_isolate,
                      v8::String::Concat(V8Value(), target_string));
}

String ScriptString::FlattenToString() {
  if (IsEmpty())
    return String();
  v8::HandleScope handle_scope(GetIsolate());
  return V8StringToWebCoreString<String>(V8Value(), kExternalize);
}

}  // namespace blink
