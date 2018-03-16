// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TraceWrapperV8String_h
#define TraceWrapperV8String_h

#include "platform/bindings/TraceWrapperBase.h"
#include "platform/bindings/TraceWrapperV8Reference.h"
#include "platform/wtf/text/WTFString.h"
#include "v8/include/v8.h"

namespace blink {

// Small shim around TraceWrapperReference<v8::String> with a few
// utility methods. Internally, v8::String is represented as string
// rope.
class PLATFORM_EXPORT TraceWrapperV8String final : public TraceWrapperBase {
  DISALLOW_COPY_AND_ASSIGN(TraceWrapperV8String);
  DISALLOW_NEW();

 public:
  TraceWrapperV8String() = default;

  bool IsEmpty() const { return string_.IsEmpty(); }
  void Clear() { string_.Clear(); }

  v8::Local<v8::String> V8Value(v8::Isolate* isolate) {
    return string_.NewLocal(isolate);
  }

  void Concat(v8::Isolate*, const String&);
  String Flatten(v8::Isolate*) const;

  void TraceWrappers(const ScriptWrappableVisitor* visitor) const override {
    visitor->TraceWrappers(string_);
  }
  const char* NameInHeapSnapshot() const override {
    return "TraceWrapperV8String";
  }

 private:
  TraceWrapperV8Reference<v8::String> string_;
};

}  // namespace blink

#endif  // TraceWrapperV8String_h
