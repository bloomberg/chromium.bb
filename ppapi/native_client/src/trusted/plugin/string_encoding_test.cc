/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <string.h>

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/plugin/string_encoding.h"


int main() {
  const char* input;
  char* output;
  size_t output_size;

  // Valid input.

  input = "Hello world, \x80 and \xff";
  CHECK(plugin::ByteStringAsUTF8(input, strlen(input), &output, &output_size));
  CHECK(strcmp(output, "Hello world, \xc2\x80 and \xc3\xbf") == 0);
  CHECK(plugin::ByteStringFromUTF8(output, output_size, &output, &output_size));
  CHECK(strcmp(output, "Hello world, \x80 and \xff") == 0);

  // Valid UTF-8, but chars too big.

  // Three-byte sequence
  // This encodes \u1000
  input = "\xe1\x80\x80";
  CHECK(!plugin::ByteStringFromUTF8(input, strlen(input),
                                    &output, &output_size));
  // This encodes \u0100
  input = "\xc4\x80";
  CHECK(!plugin::ByteStringFromUTF8(input, strlen(input),
                                    &output, &output_size));

  // Invalid UTF-8.

  // Incomplete sequence
  input = "\xc2";
  CHECK(!plugin::ByteStringFromUTF8(input, strlen(input),
                                    &output, &output_size));
  // Subsequent byte is wrong
  input = "\xc2 ";
  CHECK(!plugin::ByteStringFromUTF8(input, strlen(input),
                                    &output, &output_size));
  // Over long encoding
  // This is a non-canonical encoding for \x00
  input = "\xc0\x80";
  CHECK(!plugin::ByteStringFromUTF8(input, strlen(input),
                                    &output, &output_size));

  return 0;
}
