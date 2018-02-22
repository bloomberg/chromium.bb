// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptWrappableVisitor_h
#define ScriptWrappableVisitor_h

#include "platform/PlatformExport.h"
#include "platform/bindings/TraceWrapperBase.h"
#include "platform/heap/HeapPage.h"
#include "platform/heap/ThreadingTraits.h"
#include "platform/wtf/Deque.h"
#include "platform/wtf/Vector.h"
#include "v8/include/v8.h"

namespace blink {

template <typename T>
class DOMWrapperMap;
class HeapObjectHeader;
class ScriptWrappable;
class ScriptWrappableVisitor;
template <typename T>
class Supplement;
class TraceWrapperBase;
class TraceWrapperBaseForSupplement;
template <typename T>
class TraceWrapperV8Reference;

using HeapObjectHeaderCallback = HeapObjectHeader* (*)(const void*);
using MissedWriteBarrierCallback = void (*)();
using TraceWrappersCallback = void (*)(const ScriptWrappableVisitor*,
                                       const void* self);
using NameCallback = const char* (*)(const void* self);

#define DEFINE_TRAIT_FOR_TRACE_WRAPPERS(ClassName)                   \
  template <>                                                        \
  inline void TraceTrait<ClassName>::TraceMarkedWrapper(             \
      const ScriptWrappableVisitor* visitor, const void* t) {        \
    const ClassName* traceable = ToWrapperTracingType(t);            \
    traceable->TraceWrappers(visitor);                               \
  }

// WrapperDescriptor contains enough information to visit a
// ScriptWrappable without knowing its type statically.
// It is passed to ScriptWrappableVisitor::Visit method.
struct WrapperDescriptor {
  STACK_ALLOCATED();
  const void* traceable;
  TraceWrappersCallback trace_wrappers_callback;
  HeapObjectHeaderCallback heap_object_header_callback;
  MissedWriteBarrierCallback missed_write_barrier_callback;
  NameCallback name_callback;
};

// Abstract visitor for wrapper references in a ScriptWrappable.
// Usage:
// - Define a derived class that overrides Visit(..) methods.
// - Create an instance of the derived class: visitor.
// - Call visitor.DispatchTraceWrappers(traceable).
// DispatchTraceWrappers will invoke Visit() method for all
// wrapper references in traceable.
class PLATFORM_EXPORT ScriptWrappableVisitor {
 public:
  // Trace all wrappers of |tracable|.
  //
  // If you cannot use TraceWrapperMember & the corresponding TraceWrappers()
  // for some reason (e.g., unions using raw pointers), see
  // |TraceWrappersWithManualWriteBarrier()| below.
  template <typename T>
  void TraceWrappers(const TraceWrapperMember<T>& traceable) const {
    static_assert(sizeof(T), "T must be fully defined");
    Visit(traceable.Get());
  }

  // Enable partial tracing of objects. This is used when tracing interior
  // objects without their own header.
  template <typename T>
  void TraceWrappers(const T& traceable) const {
    static_assert(sizeof(T), "T must be fully defined");
    traceable.TraceWrappers(this);
  }

  // Only called from automatically generated bindings code.
  template <typename T>
  void TraceWrappersFromGeneratedCode(const T* traceable) const {
    Visit(traceable);
  }

  // Require all users of manual write barriers to make this explicit in their
  // |TraceWrappers| definition. Be sure to add
  // |ScriptWrappableMarkingVisitor::WriteBarrier(new_value)| after all
  // assignments to the field. Otherwise, the objects may be collected
  // prematurely.
  template <typename T>
  void TraceWrappersWithManualWriteBarrier(const T* traceable) const {
    Visit(traceable);
  }

  template <typename V8Type>
  void TraceWrappers(const TraceWrapperV8Reference<V8Type>& v8reference) const {
    Visit(v8reference.template Cast<v8::Value>());
  }

  // Trace wrappers in non-main worlds.
  void TraceWrappers(DOMWrapperMap<ScriptWrappable>*,
                     const ScriptWrappable* key) const;

  virtual void DispatchTraceWrappers(const TraceWrapperBase*) const;
  template <typename T>
  void DispatchTraceWrappers(const Supplement<T>* traceable) const {
    const TraceWrapperBaseForSupplement* base = traceable;
    DispatchTraceWrappersForSupplement(base);
  }
  // Catch all handlers needed because of mixins except for Supplement<T>.
  void DispatchTraceWrappers(const void*) const { CHECK(false); }

 protected:
  // The visitor interface. Derived visitors should override this
  // function to visit V8 references and ScriptWrappables.
  virtual void Visit(const TraceWrapperV8Reference<v8::Value>&) const = 0;
  virtual void Visit(const WrapperDescriptor&) const = 0;
  virtual void Visit(DOMWrapperMap<ScriptWrappable>*,
                     const ScriptWrappable* key) const = 0;

  template <typename T>
  static WrapperDescriptor WrapperDescriptorFor(const T* traceable) {
    return {traceable, TraceTrait<T>::TraceMarkedWrapper,
            TraceTrait<T>::GetHeapObjectHeader,
            ScriptWrappableVisitor::MissedWriteBarrier<T>,
            ScriptWrappableVisitor::NameCallback<T>};
  }

 private:
  template <typename T>
  static NOINLINE void MissedWriteBarrier() {
    NOTREACHED();
  }

  static const char* NameInHeapSnapshot(const TraceWrapperBase* traceable) {
    return traceable->NameInHeapSnapshot();
  }

  static const char* NameInHeapSnapshot(...) {
    // Default case for all non-TraceWrapperBase classes.
    return "InternalNode";
  }

  template <typename T>
  static const char* NameCallback(const void* traceable) {
    return NameInHeapSnapshot(static_cast<const T*>(traceable));
  }

  // Helper method to invoke the virtual Visit method with wrapper descriptor.
  template <typename T>
  void Visit(const T* traceable) const {
    static_assert(sizeof(T), "T must be fully defined");
    if (!traceable)
      return;
    Visit(WrapperDescriptorFor(traceable));
  }

  // Supplement-specific implementation of DispatchTraceWrappers.  The suffix of
  // "ForSupplement" is necessary not to make this member function a candidate
  // of overload resolutions.
  void DispatchTraceWrappersForSupplement(
      const TraceWrapperBaseForSupplement*) const;
};

}  // namespace blink

#endif  // ScriptWrappableVisitor_h
