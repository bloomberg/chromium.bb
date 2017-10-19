// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FetchManager_h
#define FetchManager_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "platform/heap/Handle.h"

namespace blink {

class ExecutionContext;
class FetchRequestData;
class ScriptState;

class FetchManager final : public GarbageCollected<FetchManager>,
                           public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(FetchManager);

 public:
  static FetchManager* Create(ExecutionContext*);
  ScriptPromise Fetch(ScriptState*, FetchRequestData*);
  void ContextDestroyed(ExecutionContext*) override;

  void Trace(blink::Visitor*);

 private:
  explicit FetchManager(ExecutionContext*);

  class Loader;

  // Removes loader from |m_loaders|.
  void OnLoaderFinished(Loader*);

  HeapHashSet<Member<Loader>> loaders_;
};

}  // namespace blink

#endif  // FetchManager_h
