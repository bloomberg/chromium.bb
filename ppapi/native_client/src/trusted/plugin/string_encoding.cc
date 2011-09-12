/*
 * Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/include/nacl_platform.h"
#include "native_client/src/trusted/plugin/string_encoding.h"


namespace plugin {

// PPAPI requires us to encode byte strings as UTF-8.  Unfortunately
// this is rather inefficient, in terms of both space and time.

bool ByteStringAsUTF8(const char* input, size_t input_byte_count,
                      char** result, size_t* result_byte_count) {
  // UTF-8 encoding may result in a 2x size increase at the most.
  // TODO(mseaborn): We could do a pre-scan to get the real size.
  // If we wanted to be faster, we could do a word-by-word pre-scan
  // to check for top-bit-set characters.
  size_t max_output_size = input_byte_count * 2;
  // We include a null terminator for convenience.
  char* output = reinterpret_cast<char*>(malloc(max_output_size + 1));
  if (output == NULL) {
    return false;
  }
  char* dest_ptr = output;
  for (size_t i = 0; i < input_byte_count; i++) {
    unsigned char ch = input[i];
    if (ch < 128) {
      // Code results in a one byte encoding.
      *dest_ptr++ = ch;
    } else {
      // Code results in a two byte encoding.
      *dest_ptr++ = 0xc0 | (ch >> 6);   /* Top 2 bits */
      *dest_ptr++ = 0x80 | (ch & 0x3f); /* Bottom 6 bits */
    }
  }
  *dest_ptr = 0;
  *result = output;
  *result_byte_count = dest_ptr - output;
  return true;
}

bool ByteStringFromUTF8(const char* input, size_t input_byte_count,
                        char** result, size_t* result_byte_count) {
  // The output cannot be larger than the input.
  char* output = reinterpret_cast<char*>(malloc(input_byte_count + 1));
  if (output == NULL) {
    return NULL;
  }
  char* dest_ptr = output;
  size_t i;
  for (i = 0; i < input_byte_count; ) {
    unsigned char ch = input[i];
    if ((ch & 0x80) == 0) {
      // One byte encoding.
      *dest_ptr++ = ch;
      i++;
    } else {
      if (i == input_byte_count - 1) {
        // Invalid UTF-8: incomplete sequence.
        goto fail;
      }
      // Check that this is a two byte encoding.
      // The first character must contain 110xxxxxb and the
      // second must contain 10xxxxxxb.
      unsigned char ch2 = input[i + 1];
      if ((ch & 0xe0) != 0xc0) {
        // >=2 byte encoding.
        goto fail;
      }
      if ((ch2 & 0xc0) != 0x80) {
        // Invalid UTF-8.
        goto fail;
      }
      uint32_t value = (((uint32_t) ch & 0x1f) << 6) | ((uint32_t) ch2 & 0x3f);
      if (value < 128) {
        // Invalid UTF-8: overly long encoding.
        goto fail;
      }
      if (value >= 0x100) {
        // Overly large character.  Will not fit into a byte.
        goto fail;
      }
      *dest_ptr++ = value;
      i += 2;
    }
  }
  *dest_ptr = 0;
  *result = output;
  *result_byte_count = dest_ptr - output;
  return true;
 fail:
  free(output);
  return false;
}

}  // namespace plugin
