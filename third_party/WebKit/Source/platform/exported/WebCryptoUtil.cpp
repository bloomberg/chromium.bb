// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebCryptoUtil.h"

namespace blink {

bool bigIntegerToUint(const WebVector<unsigned char>& bigInteger, unsigned& result)
{
    result = 0;
    for (size_t i = 0; i < bigInteger.size(); ++i) {
        size_t iReversed = bigInteger.size() - i - 1;

        if (iReversed >= sizeof(result) && bigInteger[i])
            return false; // Too large for unsigned int.

        result |= bigInteger[i] << 8 * iReversed;
    }
    return true;
}

} // namespace blink
