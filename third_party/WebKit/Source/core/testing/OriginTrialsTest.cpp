// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "OriginTrialsTest.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/origin_trials/OriginTrials.h"

namespace blink {

bool OriginTrialsTest::throwingAttribute(ScriptState* script_state,
                                         ExceptionState& exception_state) {
  String error_message;
  if (!OriginTrials::originTrialsSampleAPIEnabled(
          ExecutionContext::From(script_state))) {
    exception_state.ThrowDOMException(
        kNotSupportedError,
        "The Origin Trials Sample API has not been enabled in this context");
    return false;
  }
  return unconditionalAttribute();
}

DEFINE_TRACE(OriginTrialsTest) {}

}  // namespace blink
