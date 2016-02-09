// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/streams/UnderlyingSourceBase.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "core/streams/ReadableStreamController.h"
#include <v8.h>

namespace blink {

ScriptPromise UnderlyingSourceBase::startWrapper(ScriptState* scriptState, ScriptValue stream)
{
    // Cannot call start twice (e.g., cannot use the same UnderlyingSourceBase to construct multiple streams)
    ASSERT(!m_controller);

    // In ReadableStream.js, we special-case externally-controlled streams by having them pass themselves to start
    // as the first argument. This allows us to create a ReadableStreamController.

    m_controller = new ReadableStreamController(stream);

    return start(scriptState);
}

ScriptPromise UnderlyingSourceBase::start(ScriptState* scriptState)
{
    return ScriptPromise::cast(scriptState, v8::Undefined(scriptState->isolate()));
}

ScriptPromise UnderlyingSourceBase::pull(ScriptState* scriptState)
{
    return ScriptPromise::cast(scriptState, v8::Undefined(scriptState->isolate()));
}

ScriptPromise UnderlyingSourceBase::cancelWrapper(ScriptState* scriptState, ScriptValue reason)
{
    m_controller->noteHasBeenCanceled();
    return cancel(scriptState, reason);
}

ScriptPromise UnderlyingSourceBase::cancel(ScriptState* scriptState, ScriptValue reason)
{
    return ScriptPromise::cast(scriptState, v8::Undefined(scriptState->isolate()));
}

bool UnderlyingSourceBase::hasPendingActivity() const
{
    // This will return false within a finite time period _assuming_ that
    // consumers use the controller to close or error the stream.
    // Browser-created readable streams should always close or error within a
    // finite time period, due to timeouts etc.
    return m_controller && m_controller->isActive();
}

void UnderlyingSourceBase::stop()
{
    m_controller->noteHasBeenCanceled();
    m_controller.clear();
}

DEFINE_TRACE(UnderlyingSourceBase)
{
    ActiveDOMObject::trace(visitor);
    visitor->trace(m_controller);
}

} // namespace blink
