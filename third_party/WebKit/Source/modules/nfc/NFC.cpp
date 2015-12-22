// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/nfc/NFC.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/nfc/NFCMessage.h"
#include "modules/nfc/NFCPushOptions.h"

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

ScriptPromise NFC::push(ScriptState* scriptState, const NFCPushMessage& records, const NFCPushOptions& options)
{
    // TODO(shalamov): To be implemented.
    return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError));
}

ScriptPromise NFC::cancelPush(ScriptState* scriptState, const String& target)
{
    // TODO(shalamov): To be implemented.
    return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError));
}

ScriptPromise NFC::watch(ScriptState* scriptState, MessageCallback* callback, const NFCWatchOptions& options)
{
    // TODO(shalamov): To be implemented.
    return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError));
}

ScriptPromise NFC::cancelWatch(ScriptState* scriptState, long id)
{
    // TODO(shalamov): To be implemented.
    return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError));
}

ScriptPromise NFC::cancelWatch(ScriptState* scriptState)
{
    // TODO(shalamov): To be implemented.
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
