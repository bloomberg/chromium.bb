// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebCryptoUtil_h
#define WebCryptoUtil_h

#include "WebCommon.h"
#include "WebVector.h"

namespace blink {

// Converts the (big-endian) BigInteger to unsigned int. Returns true on success (if its value is not too large).
BLINK_PLATFORM_EXPORT bool bigIntegerToUint(const WebVector<unsigned char>& bigInteger, unsigned& result);

} // namespace blink

#endif // WebCryptoUtil_h
