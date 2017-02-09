// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/testing/WorkerInternals.h"

#include "bindings/core/v8/ScriptState.h"
#include "core/frame/Deprecation.h"
#include "core/frame/UseCounter.h"
#include "core/testing/OriginTrialsTest.h"

namespace blink {

WorkerInternals::~WorkerInternals() {}

WorkerInternals::WorkerInternals() {}

OriginTrialsTest* WorkerInternals::originTrialsTest() const {
  return OriginTrialsTest::create();
}

void WorkerInternals::countFeature(ScriptState* scriptState, uint32_t feature) {
  UseCounter::count(scriptState->getExecutionContext(),
                    static_cast<UseCounter::Feature>(feature));
}

void WorkerInternals::countDeprecation(ScriptState* scriptState,
                                       uint32_t feature) {
  Deprecation::countDeprecation(scriptState->getExecutionContext(),
                                static_cast<UseCounter::Feature>(feature));
}

}  // namespace blink
