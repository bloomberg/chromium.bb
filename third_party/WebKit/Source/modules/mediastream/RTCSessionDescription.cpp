/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#include "config.h"

#include "modules/mediastream/RTCSessionDescription.h"

#include "bindings/v8/Dictionary.h"
#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "public/platform/WebRTCSessionDescription.h"

namespace WebCore {

static bool verifyType(const String& type)
{
    return type == "offer" || type == "pranswer" || type == "answer";
}

PassRefPtr<RTCSessionDescription> RTCSessionDescription::create(const Dictionary& dictionary, ExceptionState& es)
{
    String type;
    bool ok = dictionary.get("type", type);
    if (!ok || !verifyType(type)) {
        es.throwUninformativeAndGenericDOMException(TypeMismatchError);
        return 0;
    }

    String sdp;
    ok = dictionary.get("sdp", sdp);
    if (!ok || sdp.isEmpty()) {
        es.throwUninformativeAndGenericDOMException(TypeMismatchError);
        return 0;
    }

    return adoptRef(new RTCSessionDescription(WebKit::WebRTCSessionDescription(type, sdp)));
}

PassRefPtr<RTCSessionDescription> RTCSessionDescription::create(WebKit::WebRTCSessionDescription webSessionDescription)
{
    return adoptRef(new RTCSessionDescription(webSessionDescription));
}

RTCSessionDescription::RTCSessionDescription(WebKit::WebRTCSessionDescription webSessionDescription)
    : m_webSessionDescription(webSessionDescription)
{
    ScriptWrappable::init(this);
}

RTCSessionDescription::~RTCSessionDescription()
{
}

String RTCSessionDescription::type()
{
    return m_webSessionDescription.type();
}

void RTCSessionDescription::setType(const String& type, ExceptionState& es)
{
    if (verifyType(type))
        m_webSessionDescription.setType(type);
    else
        es.throwUninformativeAndGenericDOMException(TypeMismatchError);
}

String RTCSessionDescription::sdp()
{
    return m_webSessionDescription.sdp();
}

void RTCSessionDescription::setSdp(const String& sdp, ExceptionState& es)
{
    m_webSessionDescription.setSDP(sdp);
}

WebKit::WebRTCSessionDescription RTCSessionDescription::webSessionDescription()
{
    return m_webSessionDescription;
}

} // namespace WebCore
