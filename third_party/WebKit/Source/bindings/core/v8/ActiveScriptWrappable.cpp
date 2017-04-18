// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ActiveScriptWrappable.h"

#include "bindings/core/v8/DOMDataStore.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/ScriptWrappableVisitor.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "core/dom/ExecutionContext.h"

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
  auto active_script_wrappables = isolate_data->ActiveScriptWrappables();
  if (!active_script_wrappables) {
    return;
  }

  for (auto active_wrappable : *active_script_wrappables) {
    if (!active_wrappable->DispatchHasPendingActivity(active_wrappable)) {
      continue;
    }

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
    if (active_wrappable->IsContextDestroyed(active_wrappable)) {
      continue;
    }

    auto script_wrappable =
        active_wrappable->ToScriptWrappable(active_wrappable);
    auto wrapper_type_info =
        const_cast<WrapperTypeInfo*>(script_wrappable->GetWrapperTypeInfo());
    visitor->RegisterV8Reference(
        std::make_pair(wrapper_type_info, script_wrappable));
  }
}

}  // namespace blink
