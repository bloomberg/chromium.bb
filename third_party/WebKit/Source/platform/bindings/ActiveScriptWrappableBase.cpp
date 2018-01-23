// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/bindings/ActiveScriptWrappableBase.h"

#include "platform/bindings/DOMDataStore.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/ScriptWrappableVisitor.h"
#include "platform/bindings/V8Binding.h"
#include "platform/bindings/V8PerIsolateData.h"

namespace blink {

ActiveScriptWrappableBase::ActiveScriptWrappableBase() {
  DCHECK(ThreadState::Current());
  v8::Isolate* isolate = ThreadState::Current()->GetIsolate();
  V8PerIsolateData* isolate_data = V8PerIsolateData::From(isolate);
  isolate_data->AddActiveScriptWrappable(this);
}

void ActiveScriptWrappableBase::TraceActiveScriptWrappables(
    v8::Isolate* isolate,
    ScriptWrappableVisitor* visitor) {
  V8PerIsolateData* isolate_data = V8PerIsolateData::From(isolate);
  const auto* active_script_wrappables = isolate_data->ActiveScriptWrappables();
  if (!active_script_wrappables)
    return;

  for (auto active_wrappable : *active_script_wrappables) {
    if (!active_wrappable->DispatchHasPendingActivity())
      continue;

    // A wrapper isn't kept alive after its ExecutionContext becomes
    // detached, even if hasPendingActivity() returns |true|. This measure
    // avoids memory leaks and has proven not to be too eager wrt
    // garbage collection of objects belonging to discarded browser contexts
    // ( https://html.spec.whatwg.org/#a-browsing-context-is-discarded )
    //
    // Consequently, an implementation of hasPendingActivity() is
    // not required to take the detached state of the associated
    // ExecutionContext into account (i.e., return |false|.) We probe
    // the detached state of the ExecutionContext via
    // |isContextDestroyed()|.
    //
    // TODO(haraken): Implement correct lifetime using traceWrapper.
    if (active_wrappable->IsContextDestroyed())
      continue;
    ScriptWrappable* script_wrappable = active_wrappable->ToScriptWrappable();
    // Notify the visitor about this script_wrappable by dispatching to the
    // corresponding visitor->Visit(script_wrappable) method.
    // Ideally, we would call visitor->TraceWrappers(script_wrappable) here,
    // but that method requires TraceWrapperMember<T>. Since we are getting
    // the script wrappable from ActiveScriptWrappables, we do not have
    // TraceWrapperMember<T> and have to use TraceWrappersFromGeneratedCode.
    visitor->TraceWrappersFromGeneratedCode(script_wrappable);
  }
}

}  // namespace blink
