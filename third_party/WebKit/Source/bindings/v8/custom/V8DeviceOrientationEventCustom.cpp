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
#include "V8DeviceOrientationEvent.h"

#include "bindings/v8/V8Binding.h"
#include "modules/device_orientation/DeviceOrientationData.h"
#include <v8.h>

namespace WebCore {

void V8DeviceOrientationEvent::initDeviceOrientationEventMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    DeviceOrientationEvent* imp = V8DeviceOrientationEvent::toNative(args.Holder());
    V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<>, type, args[0]);
    bool bubbles = args[1]->BooleanValue();
    bool cancelable = args[2]->BooleanValue();
    // If alpha, beta, gamma or absolute are null or undefined, mark them as not provided.
    // Otherwise, use the standard JavaScript conversion.
    bool alphaProvided = !isUndefinedOrNull(args[3]);
    double alpha = args[3]->NumberValue();
    bool betaProvided = !isUndefinedOrNull(args[4]);
    double beta = args[4]->NumberValue();
    bool gammaProvided = !isUndefinedOrNull(args[5]);
    double gamma = args[5]->NumberValue();
    bool absoluteProvided = !isUndefinedOrNull(args[6]);
    bool absolute = args[6]->BooleanValue();
    RefPtr<DeviceOrientationData> orientation = DeviceOrientationData::create(alphaProvided, alpha, betaProvided, beta, gammaProvided, gamma, absoluteProvided, absolute);
    imp->initDeviceOrientationEvent(type, bubbles, cancelable, orientation.get());
}

} // namespace WebCore
