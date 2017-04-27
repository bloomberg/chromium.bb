// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TraceWrapperV8Reference_h
#define TraceWrapperV8Reference_h

#include "platform/bindings/ScriptWrappableVisitor.h"

namespace blink {

/**
 * TraceWrapperV8Reference is used to trace from Blink to V8. If wrapper
 * tracing is disabled, the reference is a weak v8::Persistent. Otherwise,
 * the reference is (strongly) traced by wrapper tracing.
 *
 * TODO(mlippautz): Use a better handle type than v8::Persistent.
 */
template <typename T>
class TraceWrapperV8Reference {
 public:
  explicit TraceWrapperV8Reference(void* parent) : parent_(parent) {}

  TraceWrapperV8Reference(v8::Isolate* isolate,
                          void* parent,
                          v8::Local<T> handle)
      : parent_(parent) {
    InternalSet(isolate, handle);
    handle_.SetWeak();
  }

  ~TraceWrapperV8Reference() { Clear(); }

  void Set(v8::Isolate* isolate, v8::Local<T> handle) {
    InternalSet(isolate, handle);
    handle_.SetWeak();
  }

  template <typename P>
  void Set(v8::Isolate* isolate,
           v8::Local<T> handle,
           P* parameters,
           void (*callback)(const v8::WeakCallbackInfo<P>&),
           v8::WeakCallbackType type = v8::WeakCallbackType::kParameter) {
    InternalSet(isolate, handle);
    handle_.SetWeak(parameters, callback, type);
  }

  ALWAYS_INLINE v8::Local<T> NewLocal(v8::Isolate* isolate) const {
    return v8::Local<T>::New(isolate, handle_);
  }

  bool IsEmpty() const { return handle_.IsEmpty(); }
  void Clear() { handle_.Reset(); }
  ALWAYS_INLINE v8::Persistent<T>& Get() { return handle_; }

  template <typename S>
  const TraceWrapperV8Reference<S>& Cast() const {
    return reinterpret_cast<const TraceWrapperV8Reference<S>&>(
        const_cast<const TraceWrapperV8Reference<T>&>(*this));
  }

 private:
  inline void InternalSet(v8::Isolate* isolate, v8::Local<T> handle) {
    handle_.Reset(isolate, handle);
    ScriptWrappableVisitor::WriteBarrier(isolate, parent_, &Cast<v8::Value>());
  }

  v8::Persistent<T> handle_;
  void* parent_;
};

}  // namespace blink

#endif  // TraceWrapperV8Reference_h
