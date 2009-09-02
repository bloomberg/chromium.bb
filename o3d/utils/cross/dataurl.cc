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

// This file contains the definition of functions for dealing with data urls.

#include "utils/cross/dataurl.h"
#include "core/cross/types.h"
#include "utils/cross/base64.h"

namespace o3d {
namespace dataurl {

const char* const kEmptyDataURL = "data:,";

String ToDataURL(const String& mime_type, const void* data, size_t length) {
  String header(String("data:") + mime_type + ";base64,");
  String result(header.size() + base64::GetEncodeLength(length), ' ');
  result.replace(0, header.size(), header);
  base64::Encode(data, length, &result[header.size()]);
  return result;
}

// Decodes the data URL and stores a pointer to the data in dst_buffer
bool FromDataURL(const String& data_url,
                 scoped_array<uint8>* dst_buffer,
                 size_t* output_length,
                 String* error_string) {
    // First parse the data_url
  const String kDataHeader("data:");
  const String kBase64Header(";base64,");
  // The string has to be long enough.
  if (data_url.size() <= kDataHeader.size() + kBase64Header.size()) {
    *error_string = "Invalid formatting: The data URL is not long enough.";
    return false;
  }
  // it must start with "data:"
  if (data_url.compare(0, kDataHeader.size(), kDataHeader) != 0) {
    *error_string
        = "Invalid formatting: The data URL must start with 'data:'";
    return false;
  }
  // we only support base64 data URL's
  String::size_type data_index = data_url.find(kBase64Header);
  if (data_index == String::npos) {
    *error_string
        = "Invalid formatting: The data URL have ';base64,' in the header.";
    return false;
  }
  // The start of the data.
  data_index += kBase64Header.size();
  if (data_index >= data_url.size()) {
    *error_string
        = "Invalid formatting: There must be data in the body of the data URL.";
    return false;
  }

  // Get the length of the decoded data
  size_t input_length = data_url.size() - data_index;
  base64::DecodeStatus return_code = base64::GetDecodeLength(
      &data_url[data_index],
      input_length,
      output_length);
  if (return_code != base64::kSuccess) {
    if (return_code == base64::kPadError) {
      *error_string
          = "Invalid formatting: Padding error in the data URL data.";
    } else {
      *error_string
          = "Invalid formatting: Bad character error in the data URL data.";
    }
    return false;
  }

  dst_buffer->reset(new uint8[*output_length]);
  base64::Decode(&data_url[data_index],
                 input_length,
                 dst_buffer->get(),
                 (*output_length));

  return true;
}

}  // namespace dataurl
}  // namespace o3d

