// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/nfc/NFC.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"

namespace blink {

NFC::NFC(ExecutionContext* context, LocalFrame* frame)
    : ActiveDOMObject(context)
    , DOMWindowProperty(frame)
{
}

NFC* NFC::create(ExecutionContext* context, LocalFrame* frame)
{
    NFC* nfc = new NFC(context, frame);
    nfc->suspendIfNeeded();
    return nfc;
}

NFC::~NFC()
{
}

ScriptPromise NFC::requestAdapter(ScriptState* scriptState)
{
    // TODO(riju): To be implemented.
    return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError));
}

DEFINE_TRACE(NFC)
{
    ActiveDOMObject::trace(visitor);
    DOMWindowProperty::trace(visitor);
}

} // namespace blink
