/*
 * Copyright (c) 2009, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/modules/v8/V8SQLError.h"
#include "bindings/modules/v8/V8SQLStatementErrorCallback.h"
#include "bindings/modules/v8/V8SQLTransaction.h"
#include "core/dom/ExecutionContext.h"
#include "platform/wtf/Assertions.h"

namespace blink {

bool V8SQLStatementErrorCallback::handleEvent(SQLTransaction* transaction,
                                              SQLError* error) {
  v8::Isolate* isolate = m_scriptState->GetIsolate();
  ExecutionContext* execution_context = m_scriptState->GetExecutionContext();
  if (!execution_context || execution_context->IsContextSuspended() ||
      execution_context->IsContextDestroyed())
    return true;
  if (!m_scriptState->ContextIsValid())
    return true;
  ScriptState::Scope scope(m_scriptState.Get());

  v8::Local<v8::Value> transaction_handle =
      ToV8(transaction, m_scriptState->GetContext()->Global(), isolate);
  v8::Local<v8::Value> error_handle =
      ToV8(error, m_scriptState->GetContext()->Global(), isolate);
  ASSERT(transaction_handle->IsObject());

  v8::Local<v8::Value> argv[] = {transaction_handle, error_handle};

  v8::TryCatch exception_catcher(isolate);
  exception_catcher.SetVerbose(true);

  v8::Local<v8::Value> result;
  // FIXME: This comment doesn't make much sense given what the code is actually
  // doing.
  //
  // Step 6: If the error callback returns false, then move on to the next
  // statement, if any, or onto the next overall step otherwise. Otherwise,
  // the error callback did not return false, or there was no error callback.
  // Jump to the last step in the overall steps.
  if (!V8ScriptRunner::CallFunction(m_callback.NewLocal(isolate),
                                    m_scriptState->GetExecutionContext(),
                                    m_scriptState->GetContext()->Global(),
                                    WTF_ARRAY_LENGTH(argv), argv, isolate)
           .ToLocal(&result))
    return true;
  bool value;
  if (!result->BooleanValue(isolate->GetCurrentContext()).To(&value))
    return true;
  return value;
}

}  // namespace blink
