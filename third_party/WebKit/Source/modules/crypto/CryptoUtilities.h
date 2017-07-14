// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CryptoUtilities_h
#define CryptoUtilities_h

#include "core/typed_arrays/DOMArrayPiece.h"
#include "public/platform/WebVector.h"

namespace blink {
inline WebVector<uint8_t> CopyBytes(const DOMArrayPiece& source) {
  return WebVector<uint8_t>(static_cast<const uint8_t*>(source.Data()),
                            source.ByteLength());
}
}  // namespace blink

#endif  // CryptoUtilities_h
