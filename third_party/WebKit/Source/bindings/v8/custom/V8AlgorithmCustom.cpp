/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "config.h"
#include "V8Algorithm.h"

#include "V8AesCbcParams.h"
#include "V8AesKeyGenParams.h"
#include "V8HmacKeyParams.h"
#include "V8HmacParams.h"
#include "V8RsaKeyGenParams.h"
#include "V8RsaSsaParams.h"

namespace WebCore {

v8::Handle<v8::Object> wrap(Algorithm* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT(impl);

    // Wrap as the more derived type.
    switch (impl->type()) {
    case WebKit::WebCryptoAlgorithmParamsTypeNone:
        return V8Algorithm::createWrapper(impl, creationContext, isolate);
    case WebKit::WebCryptoAlgorithmParamsTypeAesCbcParams:
        return wrap(static_cast<AesCbcParams*>(impl), creationContext, isolate);
    case WebKit::WebCryptoAlgorithmParamsTypeAesKeyGenParams:
        return wrap(static_cast<AesKeyGenParams*>(impl), creationContext, isolate);
    case WebKit::WebCryptoAlgorithmParamsTypeHmacParams:
        return wrap(static_cast<HmacParams*>(impl), creationContext, isolate);
    case WebKit::WebCryptoAlgorithmParamsTypeHmacKeyParams:
        return wrap(static_cast<HmacKeyParams*>(impl), creationContext, isolate);
    case WebKit::WebCryptoAlgorithmParamsTypeRsaSsaParams:
        return wrap(static_cast<RsaSsaParams*>(impl), creationContext, isolate);
    case WebKit::WebCryptoAlgorithmParamsTypeRsaKeyGenParams:
        return wrap(static_cast<RsaKeyGenParams*>(impl), creationContext, isolate);
    }

    ASSERT_NOT_REACHED();
    return v8::Handle<v8::Object>();
}

} // namespace WebCore
