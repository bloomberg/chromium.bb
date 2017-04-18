// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/Worklet.h"

#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/workers/WorkletGlobalScopeProxy.h"

namespace blink {

Worklet::Worklet(LocalFrame* frame)
    : ContextLifecycleObserver(frame->GetDocument()), frame_(frame) {
  DCHECK(IsMainThread());
}

void Worklet::ContextDestroyed(ExecutionContext*) {
  DCHECK(IsMainThread());
  if (IsInitialized())
    GetWorkletGlobalScopeProxy()->TerminateWorkletGlobalScope();
  frame_ = nullptr;
}

DEFINE_TRACE(Worklet) {
  visitor->Trace(frame_);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
