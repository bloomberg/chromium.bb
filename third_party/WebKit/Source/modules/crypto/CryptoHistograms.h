// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CryptoHistograms_h
#define CryptoHistograms_h

#include "public/platform/WebCrypto.h"

namespace blink {

class ExecutionContext;
class WebCryptoAlgorithm;
class WebCryptoKeyAlgorithm;

// Increments the corresponding UseCounter for the algorithm.
void reportWebCryptoAlgorithmUsage(ExecutionContext*, const WebCryptoAlgorithm&);
void reportWebCryptoKeyAlgorithmUsage(ExecutionContext*, const WebCryptoKeyAlgorithm&);

} // namespace blink

#endif
