// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/public/multi_heap_tracer.h"
#include "gin/per_isolate_data.h"

#include <algorithm>

#include "base/logging.h"

namespace gin {

// CREATORS
MultiHeapTracer::MultiHeapTracer()
  : is_tracing_(false),
    next_id_(gin::kEmbedderUnknown),
    tracers_() {
}

MultiHeapTracer::~MultiHeapTracer() {
}

// MANIPULATORS
int MultiHeapTracer::AddHeapTracer(v8::EmbedderHeapTracer *tracer,
                                   GinEmbedder             embedder) {
  DCHECK(!is_tracing_);
  DCHECK(kEmbedderUnknown == embedder || 0 == tracers_.count(embedder));

  tracer->isolate_ = isolate_;

  int embedder_id = embedder;
  if (kEmbedderUnknown == embedder) {
    embedder_id = next_id_++;
  }

  tracers_[embedder_id] = tracer;

  return embedder_id;
}

void MultiHeapTracer::RemoveHeapTracer(int embedder_id) {
  DCHECK(!is_tracing_);
  DCHECK(1 == tracers_.count(embedder_id));

  tracers_[embedder_id]->isolate_ = nullptr;

  tracers_.erase(embedder_id);
}

void MultiHeapTracer::SetIsolate(v8::EmbedderHeapTracer *tracer) {
  tracer->isolate_ = isolate_;
}

void MultiHeapTracer::RegisterV8References(
                                const WrapperFieldPairs& wrapper_field_pairs) {
  for (auto&& id_and_tracer : tracers_) {
    id_and_tracer.second->RegisterV8References(wrapper_field_pairs);
  }
}

void MultiHeapTracer::TracePrologue(TraceFlags flags) {
  is_tracing_ = true;

  for (auto&& id_and_tracer : tracers_) {
    id_and_tracer.second->TracePrologue(flags);
  }
}

bool MultiHeapTracer::AdvanceTracing(double deadline_in_ms) {
  // To avoid starving any particular tracer, as it's conceivable that the
  // deadline will be reached before calling 'AdvanceTracing' on all tracers,
  // we visit them in a round robin fashion.

  bool done = true;

  for (auto&& id_and_tracer : tracers_) {
    if (!id_and_tracer.second->AdvanceTracing(deadline_in_ms)) {
      done = false;
    }
  }

  return done;
}

bool MultiHeapTracer::IsTracingDone() {
  for (auto&& id_and_tracer : tracers_) {
    if (!id_and_tracer.second->IsTracingDone()) {
      return false;
    }
  }

  return true;
}

void MultiHeapTracer::TraceEpilogue(TraceSummary* trace_summary) {
  is_tracing_ = false;

  for (auto&& id_and_tracer : tracers_) {
    TraceSummary child_summary;
    id_and_tracer.second->TraceEpilogue(&child_summary);

    trace_summary->time           += child_summary.time;
    trace_summary->allocated_size += child_summary.allocated_size;
  }
}

void MultiHeapTracer::EnterFinalPause(EmbedderStackState stack_state) {
  for (auto&& id_and_tracer : tracers_) {
    id_and_tracer.second->EnterFinalPause(stack_state);
  }
}

bool MultiHeapTracer::IsRootForNonTracingGC(
                                   const v8::TracedGlobal<v8::Value>& handle) {
  for (auto&& id_and_tracer : tracers_) {
    if (id_and_tracer.second->IsRootForNonTracingGC(handle)) {
      return true;
    }
  }

  return false;
}

}  // namespace gin
