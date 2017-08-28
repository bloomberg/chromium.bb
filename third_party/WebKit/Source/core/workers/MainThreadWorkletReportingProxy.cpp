// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/MainThreadWorkletReportingProxy.h"

#include "core/dom/Document.h"
#include "core/frame/Deprecation.h"
#include "core/frame/UseCounter.h"

namespace blink {

MainThreadWorkletReportingProxy::MainThreadWorkletReportingProxy(
    Document* document)
    : document_(document) {}

void MainThreadWorkletReportingProxy::CountFeature(WebFeature feature) {
  DCHECK(IsMainThread());
  // A parent document is on the same thread, so just record API use in the
  // document's UseCounter.
  UseCounter::Count(document_->GetFrame(), feature);
}

void MainThreadWorkletReportingProxy::CountDeprecation(WebFeature feature) {
  DCHECK(IsMainThread());
  // A parent document is on the same thread, so just record API use in the
  // document's UseCounter.
  Deprecation::CountDeprecation(document_->GetFrame(), feature);
}

void MainThreadWorkletReportingProxy::DidTerminateWorkerThread() {
  // MainThreadWorklet does not start and terminate a thread.
  NOTREACHED();
}

}  // namespace blink
