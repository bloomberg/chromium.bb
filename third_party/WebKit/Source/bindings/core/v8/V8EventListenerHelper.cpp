/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "bindings/core/v8/V8EventListenerHelper.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8Window.h"
#include "bindings/core/v8/V8WorkerGlobalScopeEventListener.h"

namespace blink {

EventListener* V8EventListenerHelper::GetEventListener(
    ScriptState* script_state,
    v8::Local<v8::Value> value,
    bool is_attribute,
    ListenerLookupType lookup) {
  RUNTIME_CALL_TIMER_SCOPE(script_state->GetIsolate(),
                           RuntimeCallStats::CounterId::kGetEventListener);
  if (lookup == kListenerFindOnly) {
    // Used by EventTarget::removeEventListener, specifically
    // EventTargetV8Internal::removeEventListenerMethod
    DCHECK(!is_attribute);
    return V8EventListenerHelper::ExistingEventListener(value, script_state);
  }
  if (ToLocalDOMWindow(script_state->GetContext()))
    return V8EventListenerHelper::EnsureEventListener<V8EventListener>(
        value, is_attribute, script_state);
  return V8EventListenerHelper::EnsureEventListener<
      V8WorkerGlobalScopeEventListener>(value, is_attribute, script_state);
}

}  // namespace blink
