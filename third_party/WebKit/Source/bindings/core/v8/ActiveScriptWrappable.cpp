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
  ASSERT(ThreadState::current());
  v8::Isolate* isolate = ThreadState::current()->isolate();
  V8PerIsolateData* isolateData = V8PerIsolateData::from(isolate);
  isolateData->addActiveScriptWrappable(this);
}

void ActiveScriptWrappableBase::traceActiveScriptWrappables(
    v8::Isolate* isolate,
    ScriptWrappableVisitor* visitor) {
  V8PerIsolateData* isolateData = V8PerIsolateData::from(isolate);
  auto activeScriptWrappables = isolateData->activeScriptWrappables();
  if (!activeScriptWrappables) {
    return;
  }

  for (auto activeWrappable : *activeScriptWrappables) {
    if (!activeWrappable->dispatchHasPendingActivity(activeWrappable)) {
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
    if (activeWrappable->isContextDestroyed(activeWrappable)) {
      continue;
    }

    auto scriptWrappable = activeWrappable->toScriptWrappable(activeWrappable);
    auto wrapperTypeInfo =
        const_cast<WrapperTypeInfo*>(scriptWrappable->wrapperTypeInfo());
    visitor->RegisterV8Reference(
        std::make_pair(wrapperTypeInfo, scriptWrappable));
  }
}

}  // namespace blink
