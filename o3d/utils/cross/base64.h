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

#ifndef O3D_UTILS_CROSS_BASE64_H_
#define O3D_UTILS_CROSS_BASE64_H_

#include <stddef.h>

namespace o3d {
namespace base64 {

// The possible error codes that can occur during a decoding.
enum DecodeStatus {
  kSuccess,
  kPadError,
  kBadCharError,
  kOutputOverflowError
};

// Returns the number of bytes needed to encode length bytes in base64.
size_t GetEncodeLength(size_t length);

// Encodes the src into base64 into the dst. The dst must have enough
// space to hold the result.
// Parameters:
//   src: pointer to source data.
//   length: the length of the source data
//   dst: pointer to place to store result.
void Encode(const void* src, size_t length, void* dst);

// Used to obtain the number of bytes needed to decode the src data
// from base64.
// Parameters:
//   src: pointer to the source data.
//   input_length: the length of the source data
//   decode_length: the length in bytes of the decoded data will be
//     placed here.
DecodeStatus GetDecodeLength(const void* src,
                      size_t input_length,
                      size_t* decode_length);

// Decodes the src, which should be encoded in base64, into the dst.
// dst must have enough space to hold the result. The number of bytes
// necessary can be obtained by calling GetDecodeLength()
// Parameters:
//   src: pointer to the source data
//   input_length: the length of the source data
//   dst: pointer to where the result should be stored.
//   dst_buffer_length: the size in bytes of the dst buffer. This is
//     used to check for buffer overflow.
// Returns an error code (of type DecodeStatus)
DecodeStatus Decode(const void* src,
             size_t input_length,
             void* dst,
             size_t dst_buffer_length);

}  // namespace base64
}  // namespace o3d

#endif  // O3D_UTILS_CROSS_BASE64_H_

