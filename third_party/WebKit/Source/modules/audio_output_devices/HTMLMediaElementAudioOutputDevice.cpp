// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/audio_output_devices/HTMLMediaElementAudioOutputDevice.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/ExecutionContext.h"
#include "platform/Logging.h"

namespace blink {

HTMLMediaElementAudioOutputDevice::HTMLMediaElementAudioOutputDevice()
    : m_sinkId("")
{
}

String HTMLMediaElementAudioOutputDevice::sinkId(HTMLMediaElement& element)
{
    WTF_LOG(Media, __FUNCTION__);
    HTMLMediaElementAudioOutputDevice& aodElement = HTMLMediaElementAudioOutputDevice::from(element);
    return aodElement.m_sinkId;
}

ScriptPromise HTMLMediaElementAudioOutputDevice::setSinkId(ScriptState* scriptState, HTMLMediaElement& element, const String& newSinkId)
{
    WTF_LOG(Media, __FUNCTION__);
    return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError, "Operation not supported"));
}

const char* HTMLMediaElementAudioOutputDevice::supplementName()
{
    return "HTMLMediaElementAudioOutputDevice";
}

HTMLMediaElementAudioOutputDevice& HTMLMediaElementAudioOutputDevice::from(HTMLMediaElement& element)
{
    HTMLMediaElementAudioOutputDevice* supplement = static_cast<HTMLMediaElementAudioOutputDevice*>(WillBeHeapSupplement<HTMLMediaElement>::from(element, supplementName()));
    if (!supplement) {
        supplement = new HTMLMediaElementAudioOutputDevice();
        provideTo(element, supplementName(), adoptPtrWillBeNoop(supplement));
    }
    return *supplement;
}

DEFINE_TRACE(HTMLMediaElementAudioOutputDevice)
{
    WillBeHeapSupplement<HTMLMediaElement>::trace(visitor);
}

} // namespace blink
