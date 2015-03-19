// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/crypto/CryptoHistograms.h"

#include "core/frame/UseCounter.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCryptoAlgorithm.h"
#include "public/platform/WebCryptoKeyAlgorithm.h"

namespace blink {

static UseCounter::Feature algorithmIdToFeature(WebCryptoAlgorithmId id)
{
    switch (id) {
    case WebCryptoAlgorithmIdAesCbc:
        return UseCounter::CryptoAlgorithmAesCbc;
    case WebCryptoAlgorithmIdHmac:
        return UseCounter::CryptoAlgorithmHmac;
    case WebCryptoAlgorithmIdRsaSsaPkcs1v1_5:
        return UseCounter::CryptoAlgorithmRsaSsaPkcs1v1_5;
    case WebCryptoAlgorithmIdSha1:
        return UseCounter::CryptoAlgorithmSha1;
    case WebCryptoAlgorithmIdSha256:
        return UseCounter::CryptoAlgorithmSha256;
    case WebCryptoAlgorithmIdSha384:
        return UseCounter::CryptoAlgorithmSha384;
    case WebCryptoAlgorithmIdSha512:
        return UseCounter::CryptoAlgorithmSha512;
    case WebCryptoAlgorithmIdAesGcm:
        return UseCounter::CryptoAlgorithmAesGcm;
    case WebCryptoAlgorithmIdRsaOaep:
        return UseCounter::CryptoAlgorithmRsaOaep;
    case WebCryptoAlgorithmIdAesCtr:
        return UseCounter::CryptoAlgorithmAesCtr;
    case WebCryptoAlgorithmIdAesKw:
        return UseCounter::CryptoAlgorithmAesKw;
    case WebCryptoAlgorithmIdRsaPss:
        return UseCounter::CryptoAlgorithmRsaPss;
    case WebCryptoAlgorithmIdEcdsa:
        return UseCounter::CryptoAlgorithmEcdsa;
    case WebCryptoAlgorithmIdEcdh:
        return UseCounter::CryptoAlgorithmEcdh;
    case WebCryptoAlgorithmIdHkdf:
        return UseCounter::CryptoAlgorithmHkdf;
    case WebCryptoAlgorithmIdPbkdf2:
        return UseCounter::CryptoAlgorithmPbkdf2;
    }

    ASSERT_NOT_REACHED();
    return static_cast<UseCounter::Feature>(0);
}

void reportWebCryptoAlgorithmUsage(ExecutionContext* context, const WebCryptoAlgorithm& algorithm)
{
    UseCounter::Feature feature = algorithmIdToFeature(algorithm.id());
    if (!feature)
        UseCounter::count(context, feature);
}

void reportWebCryptoKeyAlgorithmUsage(ExecutionContext* context, const WebCryptoKeyAlgorithm& algorithm)
{
    UseCounter::Feature feature = algorithmIdToFeature(algorithm.id());
    if (!feature)
        UseCounter::count(context, feature);
}

} // namespace blink
