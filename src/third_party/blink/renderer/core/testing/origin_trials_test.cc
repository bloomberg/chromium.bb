// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/origin_trials_test.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

bool OriginTrialsTest::throwingAttribute(ScriptState* script_state,
                                         ExceptionState& exception_state) {
  String error_message;
  if (!origin_trials::OriginTrialsSampleAPIEnabled(
          ExecutionContext::From(script_state))) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The Origin Trials Sample API has not been enabled in this context");
    return false;
  }
  return unconditionalAttribute();
}

}  // namespace blink
