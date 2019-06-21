// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/public/multi_heap_tracer.h"
#include "gin/per_isolate_data.h"

#include <algorithm>

#include "base/logging.h"

namespace gin {

// CLASS METHODS
MultiHeapTracer* MultiHeapTracer::From(v8::Isolate *isolate) {
    PerIsolateData *isolate_data = PerIsolateData::From(isolate);
    return isolate_data ? isolate_data->heap_tracer() : nullptr;
}

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

void MultiHeapTracer::RegisterV8References(
                                const WrapperFieldPairs& wrapper_field_pairs) {
  for (auto&& id_and_tracer : tracers_) {
    id_and_tracer.second->RegisterV8References(wrapper_field_pairs);
  }
}

void MultiHeapTracer::TracePrologue() {
  is_tracing_ = true;

  for (auto&& id_and_tracer : tracers_) {
    id_and_tracer.second->TracePrologue();
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

void MultiHeapTracer::TraceEpilogue() {
  is_tracing_ = false;

  for (auto&& id_and_tracer : tracers_) {
    id_and_tracer.second->TraceEpilogue();
  }
}

void MultiHeapTracer::EnterFinalPause(EmbedderStackState stack_state) {
  for (auto&& id_and_tracer : tracers_) {
    id_and_tracer.second->EnterFinalPause(stack_state);
  }
}

bool MultiHeapTracer::IsTracingDone() {
  for (auto&& id_and_tracer : tracers_) {
    if (!id_and_tracer.second->IsTracingDone()) {
      return false;
    }
  }

  return true;
}

}  // namespace gin
