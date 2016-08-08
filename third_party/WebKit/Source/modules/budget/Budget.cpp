// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/budget/Budget.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"

namespace blink {

Budget::Budget() {}

ScriptPromise Budget::getCost(ScriptState* scriptState, const AtomicString& actionType)
{
    // TODO(harkness): Trigger the cost calculation.
    return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError, "Not yet implemented"));
}

ScriptPromise Budget::getBudget(ScriptState* scriptState)
{
    // TODO(harkness): Trigger the budget calculation.
    return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError, "Not yet implemented"));
}

} // namespace blink
