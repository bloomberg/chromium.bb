/*
 * Copyright (c) 2013, Opera Software ASA. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Opera Software ASA nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "V8TextTrackCue.h"

#include "V8VTTCue.h"

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "core/frame/UseCounter.h"

namespace WebCore {

v8::Handle<v8::Value> toV8(TextTrackCue* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    return toV8(toVTTCue(impl), creationContext, isolate);
}

// Custom constructor to make new TextTrackCue(...) return a VTTCue. This is legacy
// compat, not per spec, and should be removed at the earliest opportunity.
void V8TextTrackCue::constructorCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "TextTrackCue", info.Holder(), info.GetIsolate());
    if (UNLIKELY(info.Length() < 3)) {
        exceptionState.throwTypeError(ExceptionMessages::notEnoughArguments(3, info.Length()));
        exceptionState.throwIfNeeded();
        return;
    }
    V8TRYCATCH_VOID(double, startTime, static_cast<double>(info[0]->NumberValue()));
    V8TRYCATCH_VOID(double, endTime, static_cast<double>(info[1]->NumberValue()));
    V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<>, text, info[2]);

    Document& document = *toDocument(currentExecutionContext());
    UseCounter::countDeprecation(document, UseCounter::TextTrackCueConstructor);

    RefPtr<VTTCue> impl = VTTCue::create(document, startTime, endTime, text);
    v8::Handle<v8::Object> wrapper = wrap(impl.get(), info.Holder(), info.GetIsolate());

    v8SetReturnValue(info, wrapper);
}

} // namespace WebCore
