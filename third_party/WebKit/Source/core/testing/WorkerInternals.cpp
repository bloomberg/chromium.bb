// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/testing/WorkerInternals.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/Deprecation.h"
#include "core/frame/UseCounter.h"
#include "core/testing/OriginTrialsTest.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

WorkerInternals::~WorkerInternals() {}

WorkerInternals::WorkerInternals() {}

OriginTrialsTest* WorkerInternals::originTrialsTest() const {
  return OriginTrialsTest::Create();
}

void WorkerInternals::countFeature(ScriptState* script_state,
                                   uint32_t feature,
                                   ExceptionState& exception_state) {
  if (static_cast<int32_t>(WebFeature::kNumberOfFeatures) <= feature) {
    exception_state.ThrowTypeError(
        "The given feature does not exist in WebFeature.");
    return;
  }
  UseCounter::Count(ExecutionContext::From(script_state),
                    static_cast<WebFeature>(feature));
}

void WorkerInternals::countDeprecation(ScriptState* script_state,
                                       uint32_t feature,
                                       ExceptionState& exception_state) {
  if (static_cast<int32_t>(WebFeature::kNumberOfFeatures) <= feature) {
    exception_state.ThrowTypeError(
        "The given feature does not exist in WebFeature.");
    return;
  }
  Deprecation::CountDeprecation(ExecutionContext::From(script_state),
                                static_cast<WebFeature>(feature));
}

void WorkerInternals::collectGarbage(ScriptState* script_state) {
  script_state->GetIsolate()->RequestGarbageCollectionForTesting(
      v8::Isolate::kFullGarbageCollection);
}

}  // namespace blink
