// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/experiments/testing/InternalsFrobulate.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/experiments/Experiments.h"

namespace blink {

namespace {
const char* kFrobulateAPIName = "Frobulate";
}

// static
bool InternalsFrobulate::frobulate(ScriptState* scriptState, Internals& internals, ExceptionState& exceptionState)
{
    String errorMessage;
    if (!Experiments::isApiEnabled(scriptState->executionContext(), kFrobulateAPIName, errorMessage)) {
        exceptionState.throwDOMException(NotSupportedError, errorMessage);
        return false;
    }
    return frobulateNoEnabledCheck(internals);
}

// static
bool InternalsFrobulate::frobulateNoEnabledCheck(Internals& internals)
{
    return true;
}

} // namespace blink
