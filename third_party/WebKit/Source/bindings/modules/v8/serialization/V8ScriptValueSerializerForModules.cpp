// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/modules/v8/serialization/V8ScriptValueSerializerForModules.h"

#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/modules/v8/V8RTCCertificate.h"

namespace blink {

bool V8ScriptValueSerializerForModules::writeDOMObject(ScriptWrappable* wrappable, ExceptionState& exceptionState)
{
    // Give the core/ implementation a chance to try first.
    // If it didn't recognize the kind of wrapper, try the modules types.
    if (V8ScriptValueSerializer::writeDOMObject(wrappable, exceptionState))
        return true;
    if (exceptionState.hadException())
        return false;

    const WrapperTypeInfo* wrapperTypeInfo = wrappable->wrapperTypeInfo();
    if (wrapperTypeInfo == &V8RTCCertificate::wrapperTypeInfo) {
        RTCCertificate* certificate = wrappable->toImpl<RTCCertificate>();
        WebRTCCertificatePEM pem = certificate->certificate().toPEM();
        writeTag(RTCCertificateTag);
        writeUTF8String(pem.privateKey());
        writeUTF8String(pem.certificate());
        return true;
    }
    return false;
}

} // namespace blink
