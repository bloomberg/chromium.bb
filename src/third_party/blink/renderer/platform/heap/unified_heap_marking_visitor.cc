// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/unified_heap_marking_visitor.h"

#include "third_party/blink/renderer/platform/bindings/dom_wrapper_map.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/unified_heap_controller.h"

namespace blink {

std::unique_ptr<UnifiedHeapMarkingVisitor> UnifiedHeapMarkingVisitor::Create(
    ThreadState* thread_state,
    MarkingMode mode,
    v8::Isolate* isolate) {
  DCHECK(thread_state);
  DCHECK(isolate);
  return std::unique_ptr<UnifiedHeapMarkingVisitor>(
      new UnifiedHeapMarkingVisitor(thread_state, mode, isolate));
}

UnifiedHeapMarkingVisitor::UnifiedHeapMarkingVisitor(ThreadState* thread_state,
                                                     MarkingMode mode,
                                                     v8::Isolate* isolate)
    : MarkingVisitor(thread_state, mode), isolate_(isolate) {}

void UnifiedHeapMarkingVisitor::WriteBarrier(
    v8::Isolate* isolate,
    const TraceWrapperV8Reference<v8::Value>& object) {
  if (!ThreadState::IsAnyIncrementalMarking())
    return;

  ThreadState* thread_state = ThreadState::Current();
  if (!thread_state->IsIncrementalMarking())
    return;

  thread_state->CurrentVisitor()->Trace(object);
}

void UnifiedHeapMarkingVisitor::WriteBarrier(
    v8::Isolate* isolate,
    DOMWrapperMap<ScriptWrappable>* wrapper_map,
    ScriptWrappable* key) {
  if (!ThreadState::IsAnyIncrementalMarking())
    return;

  ThreadState* thread_state = ThreadState::Current();
  if (!thread_state->IsIncrementalMarking())
    return;

  thread_state->CurrentVisitor()->Trace(wrapper_map, key);
}

void UnifiedHeapMarkingVisitor::WriteBarrier(
    v8::Isolate* isolate,
    const WrapperTypeInfo* wrapper_type_info,
    void* object) {
  // |object| here is either ScriptWrappable or CustomWrappable.

  if (!ThreadState::IsAnyIncrementalMarking())
    return;

  ThreadState* thread_state = ThreadState::Current();
  if (!thread_state->IsIncrementalMarking())
    return;

  wrapper_type_info->Trace(thread_state->CurrentVisitor(), object);
}

void UnifiedHeapMarkingVisitor::Visit(
    const TraceWrapperV8Reference<v8::Value>& v8_reference) {
  if (v8_reference.Get().IsEmpty())
    return;
  DCHECK(isolate_);
  v8_reference.Get().RegisterExternalReference(isolate_);
}

void UnifiedHeapMarkingVisitor::Visit(
    DOMWrapperMap<ScriptWrappable>* wrapper_map,
    const ScriptWrappable* key) {
  wrapper_map->MarkWrapper(const_cast<ScriptWrappable*>(key));
}

}  // namespace blink
