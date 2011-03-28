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

// This file contains the declaration of functions for dealing with data urls.

#ifndef O3D_UTILS_CROSS_DATAURL_H_
#define O3D_UTILS_CROSS_DATAURL_H_

#include "core/cross/types.h"
#include "base/scoped_ptr.h"

namespace o3d {
namespace dataurl {

// An empty data URL. ("data:,")
extern const char* const kEmptyDataURL;

// Creates a data URL for the given data.
String ToDataURL(const String& mime_type, const void* data, size_t length);

// Decodes the data from a data URL and stores a pointer to the data in
// dst_buffer. If an error occurs in decoding, it returns false and
// error_string will contain an error message. Otherwise, returns true.
// Parameters:
//   data_url: The data URL from which to extract the data.
//   dst_buffer: A pointer to the output data will be stored in this
//       scoped_array.
//   output_length: The length of the output data will be stored at this
//       address.
//   error_string: This will contain the error message, if an error occurs.
// Returns:
//   False if an error occurs in decoding, true otherwise.
bool FromDataURL(const String& data_url,
                 scoped_array<uint8>* dst_buffer,
                 size_t* output_length,
                 String* error_string);

}  // namespace dataurl
}  // namespace o3d

#endif  // O3D_UTILS_CROSS_DATAURL_H_

