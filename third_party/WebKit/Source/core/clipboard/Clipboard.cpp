// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/clipboard/Clipboard.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/clipboard/ClipboardPromise.h"

namespace blink {

Clipboard::Clipboard(ExecutionContext* context)
    : ContextLifecycleObserver(context) {}

ScriptPromise Clipboard::read(ScriptState* script_state) {
  return ClipboardPromise::CreateForRead(script_state);
}

ScriptPromise Clipboard::readText(ScriptState* script_state) {
  return ClipboardPromise::CreateForReadText(script_state);
}

ScriptPromise Clipboard::write(ScriptState* script_state, DataTransfer* data) {
  return ClipboardPromise::CreateForWrite(script_state, data);
}

ScriptPromise Clipboard::writeText(ScriptState* script_state,
                                   const String& data) {
  return ClipboardPromise::CreateForWriteText(script_state, data);
}

const AtomicString& Clipboard::InterfaceName() const {
  return EventTargetNames::Clipboard;
}

ExecutionContext* Clipboard::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

DEFINE_TRACE(Clipboard) {
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
