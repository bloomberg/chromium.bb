/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "bindings/modules/v8/V8DeviceOrientationEvent.h"

#include "bindings/core/v8/V8Binding.h"
#include "modules/device_orientation/DeviceOrientationData.h"
#include <v8.h>

namespace blink {

void V8DeviceOrientationEvent::initDeviceOrientationEventMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ExceptionState exceptionState(ExceptionState::ExecutionContext, "initDeviceOrientationEvent", "DeviceOrientationEvent", info.Holder(), info.GetIsolate());
    DeviceOrientationEvent* impl = V8DeviceOrientationEvent::toImpl(info.Holder());
    V8StringResource<> type(info[0]);
    if (!type.prepare())
        return;
    v8::Local<v8::Context> context = info.GetIsolate()->GetCurrentContext();
    bool bubbles;
    V8_CALL(bubbles, info[1], BooleanValue(context), return);
    bool cancelable;
    V8_CALL(cancelable, info[2], BooleanValue(context), return);
    // If alpha, beta, gamma or absolute are null or undefined, mark them as not provided.
    // Otherwise, use the standard JavaScript conversion.
    bool alphaProvided = !isUndefinedOrNull(info[3]);
    double alpha = 0;
    if (alphaProvided) {
        alpha = toRestrictedDouble(info[3], exceptionState);
        if (exceptionState.throwIfNeeded())
            return;
    }
    bool betaProvided = !isUndefinedOrNull(info[4]);
    double beta = 0;
    if (betaProvided) {
        beta = toRestrictedDouble(info[4], exceptionState);
        if (exceptionState.throwIfNeeded())
            return;
    }
    bool gammaProvided = !isUndefinedOrNull(info[5]);
    double gamma = 0;
    if (gammaProvided) {
        gamma = toRestrictedDouble(info[5], exceptionState);
        if (exceptionState.throwIfNeeded())
            return;
    }
    bool absoluteProvided = !isUndefinedOrNull(info[6]);
    bool absolute = info[6]->BooleanValue();
    DeviceOrientationData* orientation = DeviceOrientationData::create(alphaProvided, alpha, betaProvided, beta, gammaProvided, gamma, absoluteProvided, absolute);
    impl->initDeviceOrientationEvent(type, bubbles, cancelable, orientation);
}

} // namespace blink
