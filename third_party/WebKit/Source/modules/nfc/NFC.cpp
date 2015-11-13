// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/nfc/NFC.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"

namespace blink {

NFC::NFC(LocalFrame* frame)
    : LocalFrameLifecycleObserver(frame)
    , PageLifecycleObserver(frame ? frame->page() : 0)
{
}

NFC* NFC::create(LocalFrame* frame)
{
    NFC* nfc = new NFC(frame);
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

void NFC::willDetachFrameHost()
{
    // TODO(shalamov): To be implemented.
}

void NFC::pageVisibilityChanged()
{
    // TODO(shalamov): To be implemented. When visibility is lost,
    // NFC operations should be suspended.
    // https://w3c.github.io/web-nfc/#nfc-suspended
}

DEFINE_TRACE(NFC)
{
    LocalFrameLifecycleObserver::trace(visitor);
    PageLifecycleObserver::trace(visitor);
}

} // namespace blink
