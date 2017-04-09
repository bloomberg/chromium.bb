// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/crypto/CryptoHistograms.h"

#include "core/frame/UseCounter.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCryptoAlgorithm.h"
#include "public/platform/WebCryptoAlgorithmParams.h"
#include "public/platform/WebCryptoKeyAlgorithm.h"

namespace blink {

static UseCounter::Feature AlgorithmIdToFeature(WebCryptoAlgorithmId id) {
  switch (id) {
    case kWebCryptoAlgorithmIdAesCbc:
      return UseCounter::kCryptoAlgorithmAesCbc;
    case kWebCryptoAlgorithmIdHmac:
      return UseCounter::kCryptoAlgorithmHmac;
    case kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5:
      return UseCounter::kCryptoAlgorithmRsaSsaPkcs1v1_5;
    case kWebCryptoAlgorithmIdSha1:
      return UseCounter::kCryptoAlgorithmSha1;
    case kWebCryptoAlgorithmIdSha256:
      return UseCounter::kCryptoAlgorithmSha256;
    case kWebCryptoAlgorithmIdSha384:
      return UseCounter::kCryptoAlgorithmSha384;
    case kWebCryptoAlgorithmIdSha512:
      return UseCounter::kCryptoAlgorithmSha512;
    case kWebCryptoAlgorithmIdAesGcm:
      return UseCounter::kCryptoAlgorithmAesGcm;
    case kWebCryptoAlgorithmIdRsaOaep:
      return UseCounter::kCryptoAlgorithmRsaOaep;
    case kWebCryptoAlgorithmIdAesCtr:
      return UseCounter::kCryptoAlgorithmAesCtr;
    case kWebCryptoAlgorithmIdAesKw:
      return UseCounter::kCryptoAlgorithmAesKw;
    case kWebCryptoAlgorithmIdRsaPss:
      return UseCounter::kCryptoAlgorithmRsaPss;
    case kWebCryptoAlgorithmIdEcdsa:
      return UseCounter::kCryptoAlgorithmEcdsa;
    case kWebCryptoAlgorithmIdEcdh:
      return UseCounter::kCryptoAlgorithmEcdh;
    case kWebCryptoAlgorithmIdHkdf:
      return UseCounter::kCryptoAlgorithmHkdf;
    case kWebCryptoAlgorithmIdPbkdf2:
      return UseCounter::kCryptoAlgorithmPbkdf2;
  }

  ASSERT_NOT_REACHED();
  return static_cast<UseCounter::Feature>(0);
}

static void HistogramAlgorithmId(ExecutionContext* context,
                                 WebCryptoAlgorithmId algorithm_id) {
  UseCounter::Feature feature = AlgorithmIdToFeature(algorithm_id);
  if (feature)
    UseCounter::Count(context, feature);
}

void HistogramAlgorithm(ExecutionContext* context,
                        const WebCryptoAlgorithm& algorithm) {
  HistogramAlgorithmId(context, algorithm.Id());

  // Histogram any interesting parameters for the algorithm. For instance
  // the inner hash for algorithms which include one (HMAC, RSA-PSS, etc)
  switch (algorithm.ParamsType()) {
    case kWebCryptoAlgorithmParamsTypeHmacImportParams:
      HistogramAlgorithm(context, algorithm.HmacImportParams()->GetHash());
      break;
    case kWebCryptoAlgorithmParamsTypeHmacKeyGenParams:
      HistogramAlgorithm(context, algorithm.HmacKeyGenParams()->GetHash());
      break;
    case kWebCryptoAlgorithmParamsTypeRsaHashedKeyGenParams:
      HistogramAlgorithm(context, algorithm.RsaHashedKeyGenParams()->GetHash());
      break;
    case kWebCryptoAlgorithmParamsTypeRsaHashedImportParams:
      HistogramAlgorithm(context, algorithm.RsaHashedImportParams()->GetHash());
      break;
    case kWebCryptoAlgorithmParamsTypeEcdsaParams:
      HistogramAlgorithm(context, algorithm.EcdsaParams()->GetHash());
      break;
    case kWebCryptoAlgorithmParamsTypeHkdfParams:
      HistogramAlgorithm(context, algorithm.HkdfParams()->GetHash());
      break;
    case kWebCryptoAlgorithmParamsTypePbkdf2Params:
      HistogramAlgorithm(context, algorithm.Pbkdf2Params()->GetHash());
      break;
    case kWebCryptoAlgorithmParamsTypeEcdhKeyDeriveParams:
    case kWebCryptoAlgorithmParamsTypeNone:
    case kWebCryptoAlgorithmParamsTypeAesCbcParams:
    case kWebCryptoAlgorithmParamsTypeAesGcmParams:
    case kWebCryptoAlgorithmParamsTypeAesKeyGenParams:
    case kWebCryptoAlgorithmParamsTypeRsaOaepParams:
    case kWebCryptoAlgorithmParamsTypeAesCtrParams:
    case kWebCryptoAlgorithmParamsTypeRsaPssParams:
    case kWebCryptoAlgorithmParamsTypeEcKeyGenParams:
    case kWebCryptoAlgorithmParamsTypeEcKeyImportParams:
    case kWebCryptoAlgorithmParamsTypeAesDerivedKeyParams:
      break;
  }
}

void HistogramKey(ExecutionContext* context, const WebCryptoKey& key) {
  const WebCryptoKeyAlgorithm& algorithm = key.Algorithm();

  HistogramAlgorithmId(context, algorithm.Id());

  // Histogram any interesting parameters that are attached to the key. For
  // instance the inner hash being used for HMAC.
  switch (algorithm.ParamsType()) {
    case kWebCryptoKeyAlgorithmParamsTypeHmac:
      HistogramAlgorithm(context, algorithm.HmacParams()->GetHash());
      break;
    case kWebCryptoKeyAlgorithmParamsTypeRsaHashed:
      HistogramAlgorithm(context, algorithm.RsaHashedParams()->GetHash());
      break;
    case kWebCryptoKeyAlgorithmParamsTypeNone:
    case kWebCryptoKeyAlgorithmParamsTypeAes:
    case kWebCryptoKeyAlgorithmParamsTypeEc:
      break;
  }
}

void HistogramAlgorithmAndKey(ExecutionContext* context,
                              const WebCryptoAlgorithm& algorithm,
                              const WebCryptoKey& key) {
  // Note that the algorithm ID for |algorithm| and |key| will usually be the
  // same. This is OK because UseCounter only increments things once per the
  // context.
  HistogramAlgorithm(context, algorithm);
  HistogramKey(context, key);
}

}  // namespace blink
