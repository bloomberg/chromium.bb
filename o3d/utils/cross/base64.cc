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

// This function actually does the decoding.
// Parameters:
//   src_ptr: pointer to the source data
//   input_length: The length in bytes of the source data.
//   dst_ptr: pointer to where the output data should be stored.
//   dst_buffer_length: the size in bytes of the dst_ptr buffer. This
//     is used to check for overflow. Not used if write_destination is
//     false.
//   output_length: The output length (in bytes) will be stored here if
//     it is not null.
//   write_destination: If true, actually write to the output. Otherwise,
//     the length of the output will be determined only.
DecodeStatus PerformDecode(const void* src_ptr,
                    size_t input_length,
                    void* dst_ptr,
                    size_t dst_buffer_length,
                    size_t* output_length,
                    bool write_destination) {
  const int kDecodePad = -2;

  static const int8 kDecodeData[] = {
    62, -1, -1, -1, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, kDecodePad, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6, 7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51
  };

  uint8* dst = reinterpret_cast<uint8*>(dst_ptr);
  const uint8* dst_start = reinterpret_cast<const uint8*>(dst_ptr);
  const uint8* dst_end = dst_start + dst_buffer_length;
  const uint8* src = reinterpret_cast<const uint8*>(src_ptr);
  bool pad_two = false;
  bool pad_three = false;
  const uint8* end = src + input_length;
  while (src < end) {
    uint8 bytes[4];
    int byte = 0;
    do {
      uint8 src_byte = *src++;
      if (src_byte == 0)
        goto goHome;
      if (src_byte <= ' ')
        continue;  // treat as white space
      if (src_byte < '+' || src_byte > 'z')
        return kBadCharError;
      int8 decoded = kDecodeData[src_byte - '+'];
      bytes[byte] = decoded;
      if (decoded < 0) {
        if (decoded == kDecodePad)
          goto handlePad;
        return kBadCharError;
      } else {
        byte++;
      }
      if (*src)
        continue;
      if (byte == 0)
        goto goHome;
      if (byte == 4)
        break;
 handlePad:
      if (byte < 2)
        return kPadError;
      pad_three = true;
      if (byte == 2)
        pad_two = true;
      break;
    } while (byte < 4);
    int two, three;
    if (write_destination) {
      int one = (bytes[0] << 2) & 0xFF;
      two = bytes[1];
      one |= two >> 4;
      two = (two << 4) & 0xFF;
      three = bytes[2];
      two |= three >> 2;
      three = (three << 6) & 0xFF;
      three |= bytes[3];
      O3D_ASSERT(one < 256 && two < 256 && three < 256);
      if (dst >= dst_end) {
        return kOutputOverflowError;
      }
      *dst = one;
    }
    dst++;
    if (pad_two)
      break;
    if (write_destination) {
      if (dst >= dst_end) {
        return kOutputOverflowError;
      }
      *dst = two;
    }
    dst++;
    if (pad_three)
      break;
    if (write_destination) {
      if (dst >= dst_end) {
        return kOutputOverflowError;
      }
      *dst = three;
    }
    dst++;
  }
 goHome:
  if (output_length) {
    *output_length = dst - dst_start;
  }
  return kSuccess;
}

// Returns the number of bytes of output data after decoding from base64.
DecodeStatus GetDecodeLength(const void* src,
                      size_t input_length,
                      size_t* decode_length) {
  return PerformDecode(src, input_length, NULL, 0, decode_length, false);
}

// Decodes the src data from base64 and stores the result in dst.
DecodeStatus Decode(const void* src,
             size_t input_length,
             void* dst,
             size_t dst_buffer_length) {
  return PerformDecode(src, input_length, dst, dst_buffer_length, NULL, true);
}

}  // namespace base64
}  // namespace o3d

