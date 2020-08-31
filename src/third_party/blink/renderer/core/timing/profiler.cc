// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/profiler.h"

#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/timing/profiler_group.h"

namespace blink {

void Profiler::Trace(Visitor* visitor) {
  visitor->Trace(profiler_group_);
  visitor->Trace(script_state_);
  ScriptWrappable::Trace(visitor);
}

void Profiler::DisposeAsync() {
  if (profiler_group_) {
    // It's safe to touch |profiler_group_| in Profiler's pre-finalizer as
    // |profiler_group_| is guaranteed to outlive the Profiler, if set. This is
    // due to ProfilerGroup nulling out this field for all attached Profilers
    // prior to destruction.
    profiler_group_->CancelProfilerAsync(script_state_, this);
    profiler_group_ = nullptr;
  }
}

ScriptPromise Profiler::stop(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!stopped()) {
    DCHECK(profiler_group_);
    profiler_group_->StopProfiler(script_state, this, resolver);
    profiler_group_ = nullptr;
  } else {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, "Profiler already stopped."));
  }

  return promise;
}

}  // namespace blink
