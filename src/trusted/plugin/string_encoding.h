/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_STRING_ENCODING_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_STRING_ENCODING_H_

#include <stdlib.h>

namespace plugin {

bool ByteStringAsUTF8(const char* input, size_t input_byte_count,
                      char** result, size_t* result_byte_count);
bool ByteStringFromUTF8(const char* input, size_t input_byte_count,
                        char** result, size_t* result_byte_count);

}

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_STRING_ENCODING_H_
