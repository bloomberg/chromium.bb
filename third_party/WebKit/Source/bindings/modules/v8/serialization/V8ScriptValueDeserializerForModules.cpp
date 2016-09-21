// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/modules/v8/serialization/V8ScriptValueDeserializerForModules.h"

#include "modules/peerconnection/RTCCertificate.h"
#include "public/platform/Platform.h"
#include "public/platform/WebRTCCertificateGenerator.h"

namespace blink {

ScriptWrappable* V8ScriptValueDeserializerForModules::readDOMObject(SerializationTag tag)
{
    // Give the core/ implementation a chance to try first.
    // If it didn't recognize the kind of wrapper, try the modules types.
    if (ScriptWrappable* wrappable = V8ScriptValueDeserializer::readDOMObject(tag))
        return wrappable;

    switch (tag) {
    case RTCCertificateTag: {
        String pemPrivateKey;
        String pemCertificate;
        if (!readUTF8String(&pemPrivateKey) || !readUTF8String(&pemCertificate))
            return nullptr;
        std::unique_ptr<WebRTCCertificateGenerator> certificateGenerator(
            Platform::current()->createRTCCertificateGenerator());
        std::unique_ptr<WebRTCCertificate> certificate =
            certificateGenerator->fromPEM(pemPrivateKey, pemCertificate);
        return new RTCCertificate(std::move(certificate));
    }
    default:
        break;
    }
    return nullptr;
}

} // namespace blink
