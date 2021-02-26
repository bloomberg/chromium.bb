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

#include "third_party/blink/renderer/platform/bindings/v8_binding.h"

#include "third_party/blink/renderer/platform/bindings/v8_binding_macros.h"

namespace blink {

v8::Local<v8::Function> GetBoundFunction(v8::Local<v8::Function> function) {
  v8::Local<v8::Value> bound_function = function->GetBoundFunction();
  return bound_function->IsFunction()
             ? v8::Local<v8::Function>::Cast(bound_function)
             : function;
}

v8::Local<v8::Value> FreezeV8Object(v8::Local<v8::Value> value,
                                    v8::Isolate* isolate) {
  value.As<v8::Object>()
      ->SetIntegrityLevel(isolate->GetCurrentContext(),
                          v8::IntegrityLevel::kFrozen)
      .ToChecked();
  return value;
}

String GetCurrentScriptUrl(int max_stack_depth) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  DCHECK(isolate);
  if (!isolate->InContext())
    return String();

  // CurrentStackTrace is 10x faster than CaptureStackTrace if all that you need
  // is the url of the script at the top of the stack. See crbug.com/1057211 for
  // more detail.
  v8::Local<v8::StackTrace> stack_trace =
      v8::StackTrace::CurrentStackTrace(isolate, max_stack_depth);
  if (stack_trace.IsEmpty())
    return String();
  for (int i = 0, frame_count = stack_trace->GetFrameCount(); i < frame_count;
       ++i) {
    v8::Local<v8::StackFrame> frame = stack_trace->GetFrame(isolate, i);
    if (frame.IsEmpty())
      continue;
    v8::Local<v8::String> script_name = frame->GetScriptNameOrSourceURL();
    if (script_name.IsEmpty() || !script_name->Length())
      continue;
    return ToCoreString(script_name);
  }
  return String();
}

}  // namespace blink
