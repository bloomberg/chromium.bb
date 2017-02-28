// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8CrossOriginSetterInfo_h
#define V8CrossOriginSetterInfo_h

#include "v8/include/v8.h"
#include "wtf/Allocator.h"

namespace blink {

// Simple adapter that is used in place of v8::PropertyCallbackInfo for setters
// that are accessible cross-origin. This is needed because a named access check
// interceptor takes a v8::PropertyCallbackInfo<v8::Value> argument, while a
// normal setter interceptor takes a v8::PropertyCallbackInfo<void> argument.
//
// Since the generated bindings only care about two fields (the isolate and the
// holder), the generated bindings just substitutes this for the normal
// v8::PropertyCallbackInfo argument, so the same generated function can be used
// to handle intercepted cross-origin sets and normal sets.
class V8CrossOriginSetterInfo {
  STACK_ALLOCATED();

 public:
  V8CrossOriginSetterInfo(v8::Isolate* isolate, v8::Local<v8::Object> holder)
      : m_isolate(isolate), m_holder(holder) {}

  v8::Isolate* GetIsolate() const { return m_isolate; }
  v8::Local<v8::Object> Holder() const { return m_holder; }

 private:
  v8::Isolate* m_isolate;
  v8::Local<v8::Object> m_holder;
};

}  // namespace blink

#endif  // V8CrossOriginSetterInfo_h
