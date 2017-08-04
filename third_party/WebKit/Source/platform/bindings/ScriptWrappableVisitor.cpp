// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/bindings/ScriptWrappableVisitor.h"

#include "platform/bindings/ActiveScriptWrappable.h"
#include "platform/bindings/DOMWrapperWorld.h"
#include "platform/bindings/ScopedPersistent.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/ScriptWrappableVisitorVerifier.h"
#include "platform/bindings/TraceWrapperV8Reference.h"
#include "platform/bindings/V8PerIsolateData.h"
#include "platform/bindings/WrapperTypeInfo.h"
#include "platform/heap/HeapCompact.h"
#include "platform/heap/HeapPage.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/wtf/AutoReset.h"
#include "platform/wtf/CurrentTime.h"
#include "public/platform/Platform.h"

namespace blink {

ScriptWrappableVisitor::~ScriptWrappableVisitor() {}

void ScriptWrappableVisitor::TracePrologue() {
  // This CHECK ensures that wrapper tracing is not started from scopes
  // that forbid GC execution, e.g., constructors.
  CHECK(ThreadState::Current());
  CHECK(!ThreadState::Current()->IsWrapperTracingForbidden());
  PerformCleanup();

  CHECK(!tracing_in_progress_);
  CHECK(!should_cleanup_);
  CHECK(headers_to_unmark_.IsEmpty());
  CHECK(marking_deque_.IsEmpty());
  CHECK(verifier_deque_.IsEmpty());
  tracing_in_progress_ = true;
  ThreadState::Current()->SetWrapperTracingInProgress(true);
}

void ScriptWrappableVisitor::EnterFinalPause() {
  CHECK(ThreadState::Current());
  CHECK(!ThreadState::Current()->IsWrapperTracingForbidden());
  ActiveScriptWrappableBase::TraceActiveScriptWrappables(isolate_, this);
}

void ScriptWrappableVisitor::TraceEpilogue() {
  CHECK(ThreadState::Current());
  CHECK(!ThreadState::Current()->IsWrapperTracingForbidden());
  DCHECK(marking_deque_.IsEmpty());
#if DCHECK_IS_ON()
  ScriptWrappableVisitorVerifier verifier;
  for (auto& marking_data : verifier_deque_) {
    marking_data.TraceWrappers(&verifier);
  }
#endif

  should_cleanup_ = true;
  tracing_in_progress_ = false;
  ThreadState::Current()->SetWrapperTracingInProgress(false);
  ScheduleIdleLazyCleanup();
}

void ScriptWrappableVisitor::AbortTracing() {
  CHECK(ThreadState::Current());
  should_cleanup_ = true;
  tracing_in_progress_ = false;
  ThreadState::Current()->SetWrapperTracingInProgress(false);
  PerformCleanup();
}

size_t ScriptWrappableVisitor::NumberOfWrappersToTrace() {
  CHECK(ThreadState::Current());
  return marking_deque_.size();
}

void ScriptWrappableVisitor::PerformCleanup() {
  if (!should_cleanup_)
    return;

  CHECK(!tracing_in_progress_);
  for (auto header : headers_to_unmark_) {
    // Dead objects residing in the marking deque may become invalid due to
    // minor garbage collections and are therefore set to nullptr. We have
    // to skip over such objects.
    if (header)
      header->UnmarkWrapperHeader();
  }

  headers_to_unmark_.clear();
  marking_deque_.clear();
  verifier_deque_.clear();
  should_cleanup_ = false;
}

void ScriptWrappableVisitor::ScheduleIdleLazyCleanup() {
  // Some threads (e.g. PPAPI thread) don't have a scheduler.
  if (!Platform::Current()->CurrentThread()->Scheduler())
    return;

  if (idle_cleanup_task_scheduled_)
    return;

  Platform::Current()->CurrentThread()->Scheduler()->PostIdleTask(
      BLINK_FROM_HERE, WTF::Bind(&ScriptWrappableVisitor::PerformLazyCleanup,
                                 WTF::Unretained(this)));
  idle_cleanup_task_scheduled_ = true;
}

void ScriptWrappableVisitor::PerformLazyCleanup(double deadline_seconds) {
  idle_cleanup_task_scheduled_ = false;

  if (!should_cleanup_)
    return;

  TRACE_EVENT1("blink_gc,devtools.timeline",
               "ScriptWrappableVisitor::performLazyCleanup",
               "idleDeltaInSeconds",
               deadline_seconds - MonotonicallyIncreasingTime());

  const int kDeadlineCheckInterval = 2500;
  int processed_wrapper_count = 0;
  for (auto it = headers_to_unmark_.rbegin();
       it != headers_to_unmark_.rend();) {
    auto header = *it;
    // Dead objects residing in the marking deque may become invalid due to
    // minor garbage collections and are therefore set to nullptr. We have
    // to skip over such objects.
    if (header)
      header->UnmarkWrapperHeader();

    ++it;
    headers_to_unmark_.pop_back();

    processed_wrapper_count++;
    if (processed_wrapper_count % kDeadlineCheckInterval == 0) {
      if (deadline_seconds <= MonotonicallyIncreasingTime()) {
        ScheduleIdleLazyCleanup();
        return;
      }
    }
  }

  // Unmarked all headers.
  CHECK(headers_to_unmark_.IsEmpty());
  marking_deque_.clear();
  verifier_deque_.clear();
  should_cleanup_ = false;
}

void ScriptWrappableVisitor::RegisterV8Reference(
    const std::pair<void*, void*>& internal_fields) {
  if (!tracing_in_progress_) {
    return;
  }

  WrapperTypeInfo* wrapper_type_info =
      reinterpret_cast<WrapperTypeInfo*>(internal_fields.first);
  if (wrapper_type_info->gin_embedder != gin::GinEmbedder::kEmbedderBlink) {
    return;
  }
  DCHECK(wrapper_type_info->wrapper_class_id == WrapperTypeInfo::kNodeClassId ||
         wrapper_type_info->wrapper_class_id ==
             WrapperTypeInfo::kObjectClassId);

  ScriptWrappable* script_wrappable =
      reinterpret_cast<ScriptWrappable*>(internal_fields.second);

  wrapper_type_info->TraceWrappers(this, script_wrappable);
}

void ScriptWrappableVisitor::RegisterV8References(
    const std::vector<std::pair<void*, void*>>&
        internal_fields_of_potential_wrappers) {
  CHECK(ThreadState::Current());
  // TODO(hlopko): Visit the vector in the V8 instead of passing it over if
  // there is no performance impact
  for (auto& pair : internal_fields_of_potential_wrappers) {
    RegisterV8Reference(pair);
  }
}

bool ScriptWrappableVisitor::AdvanceTracing(
    double deadline_in_ms,
    v8::EmbedderHeapTracer::AdvanceTracingActions actions) {
  // Do not drain the marking deque in a state where we can generally not
  // perform a GC. This makes sure that TraceTraits and friends find
  // themselves in a well-defined environment, e.g., properly set up vtables.
  CHECK(ThreadState::Current());
  CHECK(!ThreadState::Current()->IsWrapperTracingForbidden());
  CHECK(tracing_in_progress_);
  WTF::AutoReset<bool>(&advancing_tracing_, true);
  while (actions.force_completion ==
             v8::EmbedderHeapTracer::ForceCompletionAction::FORCE_COMPLETION ||
         WTF::MonotonicallyIncreasingTimeMS() < deadline_in_ms) {
    if (marking_deque_.IsEmpty()) {
      return false;
    }
    marking_deque_.TakeFirst().TraceWrappers(this);
  }
  return true;
}

bool ScriptWrappableVisitor::MarkWrapperHeader(HeapObjectHeader* header) const {
  if (header->IsWrapperHeaderMarked())
    return false;

  // Verify that no compactable & movable objects are slated for
  // lazy unmarking.
  DCHECK(!HeapCompact::IsCompactableArena(
      PageFromObject(header)->Arena()->ArenaIndex()));
  header->MarkWrapperHeader();
  headers_to_unmark_.push_back(header);
  return true;
}

void ScriptWrappableVisitor::MarkWrappersInAllWorlds(
    const ScriptWrappable* script_wrappable) const {
  DOMWrapperWorld::MarkWrappersInAllWorlds(
      const_cast<ScriptWrappable*>(script_wrappable), this);
}

void ScriptWrappableVisitor::WriteBarrier(
    v8::Isolate* isolate,
    const TraceWrapperV8Reference<v8::Value>* dst_object) {
  if (!dst_object || dst_object->IsEmpty() ||
      !ThreadState::Current()->WrapperTracingInProgress()) {
    return;
  }

  // Conservatively assume that the source object containing |dst_object| is
  // marked.
  CurrentVisitor(isolate)->MarkWrapper(
      &(const_cast<TraceWrapperV8Reference<v8::Value>*>(dst_object)->Get()));
}

void ScriptWrappableVisitor::WriteBarrier(
    v8::Isolate* isolate,
    const v8::Persistent<v8::Object>* dst_object) {
  if (!dst_object || dst_object->IsEmpty() ||
      !ThreadState::Current()->WrapperTracingInProgress()) {
    return;
  }
  CurrentVisitor(isolate)->MarkWrapper(&(dst_object->As<v8::Value>()));
}

void ScriptWrappableVisitor::TraceWrappers(
    const TraceWrapperV8Reference<v8::Value>& traced_wrapper) const {
  MarkWrapper(
      &(const_cast<TraceWrapperV8Reference<v8::Value>&>(traced_wrapper).Get()));
}

void ScriptWrappableVisitor::MarkWrapper(
    const v8::PersistentBase<v8::Value>* handle) const {
  // The write barrier may try to mark a wrapper because cleanup is still
  // delayed. Bail out in this case. We also allow unconditional marking which
  // requires us to bail out here when tracing is not in progress.
  if (!tracing_in_progress_ || handle->IsEmpty())
    return;
  handle->RegisterExternalReference(isolate_);
}

void ScriptWrappableVisitor::DispatchTraceWrappers(
    const TraceWrapperBase* wrapper_base) const {
  wrapper_base->TraceWrappers(this);
}

void ScriptWrappableVisitor::InvalidateDeadObjectsInMarkingDeque() {
  for (auto it = marking_deque_.begin(); it != marking_deque_.end(); ++it) {
    auto& marking_data = *it;
    if (marking_data.ShouldBeInvalidated()) {
      marking_data.Invalidate();
    }
  }
  for (auto it = verifier_deque_.begin(); it != verifier_deque_.end(); ++it) {
    auto& marking_data = *it;
    if (marking_data.ShouldBeInvalidated()) {
      marking_data.Invalidate();
    }
  }
  for (auto it = headers_to_unmark_.begin(); it != headers_to_unmark_.end();
       ++it) {
    auto header = *it;
    if (header && !header->IsMarked()) {
      *it = nullptr;
    }
  }
}

void ScriptWrappableVisitor::InvalidateDeadObjectsInMarkingDeque(
    v8::Isolate* isolate) {
  ScriptWrappableVisitor* script_wrappable_visitor =
      V8PerIsolateData::From(isolate)->GetScriptWrappableVisitor();
  if (script_wrappable_visitor)
    script_wrappable_visitor->InvalidateDeadObjectsInMarkingDeque();
}

void ScriptWrappableVisitor::PerformCleanup(v8::Isolate* isolate) {
  ScriptWrappableVisitor* script_wrappable_visitor =
      V8PerIsolateData::From(isolate)->GetScriptWrappableVisitor();
  if (script_wrappable_visitor)
    script_wrappable_visitor->PerformCleanup();
}

WrapperVisitor* ScriptWrappableVisitor::CurrentVisitor(v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->GetScriptWrappableVisitor();
}

}  // namespace blink
