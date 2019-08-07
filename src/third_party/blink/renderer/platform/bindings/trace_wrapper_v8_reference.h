// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_TRACE_WRAPPER_V8_REFERENCE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_TRACE_WRAPPER_V8_REFERENCE_H_

#include <utility>

#include "base/macros.h"
#include "third_party/blink/renderer/platform/heap/unified_heap_marking_visitor.h"
#include "v8/include/v8.h"

namespace blink {

/**
 * TraceWrapperV8Reference is used to hold references from Blink to V8 that are
 * known to both garbage collectors. The reference is a regular traced reference
 * for wrapper tracing as well as unified heap garbage collections.
 */
template <typename T>
class TraceWrapperV8Reference {
 public:
  TraceWrapperV8Reference() = default;

  TraceWrapperV8Reference(v8::Isolate* isolate, v8::Local<T> handle) {
    InternalSet(isolate, handle);
  }

  ~TraceWrapperV8Reference() { Clear(); }

  bool operator==(const TraceWrapperV8Reference& other) const {
    return handle_ == other.handle_;
  }

  void Set(v8::Isolate* isolate, v8::Local<T> handle) {
    InternalSet(isolate, handle);
  }

  ALWAYS_INLINE v8::Local<T> NewLocal(v8::Isolate* isolate) const {
    return handle_.Get(isolate);
  }

  bool IsEmpty() const { return handle_.IsEmpty(); }
  void Clear() { handle_.Reset(); }
  ALWAYS_INLINE const v8::TracedGlobal<T>& Get() const { return handle_; }
  ALWAYS_INLINE v8::TracedGlobal<T>& Get() { return handle_; }

  template <typename S>
  const TraceWrapperV8Reference<S>& Cast() const {
    static_assert(std::is_base_of<S, T>::value, "T must inherit from S");
    return reinterpret_cast<const TraceWrapperV8Reference<S>&>(
        const_cast<const TraceWrapperV8Reference<T>&>(*this));
  }

  template <typename S>
  const TraceWrapperV8Reference<S>& UnsafeCast() const {
    return reinterpret_cast<const TraceWrapperV8Reference<S>&>(
        const_cast<const TraceWrapperV8Reference<T>&>(*this));
  }

  // Move support.
  TraceWrapperV8Reference(TraceWrapperV8Reference&& other)
      : handle_(std::move(other.handle_)) {
    WriteBarrier();
  }

  template <class S>
  TraceWrapperV8Reference& operator=(TraceWrapperV8Reference<S>&& rhs) {
    handle_ = std::move(rhs.handle_);
    WriteBarrier();
    return *this;
  }

 protected:
  ALWAYS_INLINE void InternalSet(v8::Isolate* isolate, v8::Local<T> handle) {
    handle_.Reset(isolate, handle);
    UnifiedHeapMarkingVisitor::WriteBarrier(UnsafeCast<v8::Value>());
  }

  ALWAYS_INLINE void WriteBarrier() const {
    UnifiedHeapMarkingVisitor::WriteBarrier(UnsafeCast<v8::Value>());
  }

  v8::TracedGlobal<T> handle_;

  DISALLOW_COPY_AND_ASSIGN(TraceWrapperV8Reference);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_TRACE_WRAPPER_V8_REFERENCE_H_
