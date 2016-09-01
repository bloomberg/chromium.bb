// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationReceiver.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalFrame.h"
#include "modules/presentation/PresentationConnectionList.h"

namespace blink {

PresentationReceiver::PresentationReceiver(LocalFrame* frame)
    : DOMWindowProperty(frame)
{
}

ScriptPromise PresentationReceiver::connectionList(ScriptState* scriptState)
{
    if (!m_connectionListProperty)
        m_connectionListProperty = new ConnectionListProperty(scriptState->getExecutionContext(), this, ConnectionListProperty::Loaded);

    return m_connectionListProperty->promise(scriptState->world());
}

DEFINE_TRACE(PresentationReceiver)
{
    visitor->trace(m_connectionListProperty);
    DOMWindowProperty::trace(visitor);
}

} // namespace blink
