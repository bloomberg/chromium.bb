/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
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

// This file contains the declaration of functions for dealing with base64
// encoding and decoding

#include "utils/cross/base64.h"
#include "core/cross/types.h"

namespace o3d {
namespace base64 {

size_t GetEncodeLength(size_t length) {
  return (length + 2) / 3 * 4;
}

void Encode(const void* src_ptr, size_t length, void* dst_ptr) {
  const int kEncodePad = 64;

  static const char kEncode[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "0123456789+/=";

  const uint8* src = reinterpret_cast<const uint8*>(src_ptr);
  uint8* dst = reinterpret_cast<uint8*>(dst_ptr);
  if (dst) {
    size_t remainder = length % 3;
    const uint8* end = &src[length - remainder];
    while (src < end) {
      unsigned a = *src++;
      unsigned b = *src++;
      unsigned c = *src++;
      unsigned d = c & 0x3F;
      c = (c >> 6 | b << 2) & 0x3F;
      b = (b >> 4 | a << 4) & 0x3F;
      a = a >> 2;
      *dst++ = kEncode[a];
      *dst++ = kEncode[b];
      *dst++ = kEncode[c];
      *dst++ = kEncode[d];
    }
    if (remainder > 0) {
      unsigned k1 = 0;
      unsigned k2 = kEncodePad;
      unsigned a =  *src++;
      if (remainder == 2) {
        int b = *src++;
        k1 = b >> 4;
        k2 = (b << 2) & 0x3F;
      }
      *dst++ = kEncode[a >> 2];
      *dst++ = kEncode[(k1 | a << 4) & 0x3F];
      *dst++ = kEncode[k2];
      *dst++ = kEncode[kEncodePad];
    }
  }
}

}  // namespace base64
}  // namespace o3d

