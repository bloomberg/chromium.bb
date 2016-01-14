// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/worklet/Worklet.h"

#include "bindings/core/v8/V8Binding.h"

namespace blink {

// static
Worklet* Worklet::create(ExecutionContext* executionContext)
{
    return new Worklet(executionContext);
}

Worklet::Worklet(ExecutionContext* executionContext)
    : m_workletGlobalScope(WorkletGlobalScope::create(executionContext->url(), executionContext->userAgent(), toIsolate(executionContext)))
{
}

DEFINE_TRACE(Worklet)
{
    visitor->trace(m_workletGlobalScope);
}

} // namespace blink
