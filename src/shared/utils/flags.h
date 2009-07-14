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

/*
 * Some useful routines to handle command line arguments.
 */

#ifndef NATIVE_CLIENT_SRC_SHARED_UTILS_FLAGS_H__
#define NATIVE_CLIENT_SRC_SHARED_UTILS_FLAGS_H__

#include "native_client/src/shared/utils/types.h"

EXTERN_C_BEGIN

/*
 * Attempts to parse a boolean command line argument.
 * Returns TRUE iff able to parse and set the flag.
 *
 * name - The (command line) flag name.
 * arg - The command line argument to process.
 * flag - Pointer to the corresponding boolean flag.
 */
Bool GrokBoolFlag(const char* name,
                  const char* arg,
                  Bool* flag);

/*
 * Attempts to parse an integer command line argment.
 * Returns TRUE iff able to parse and set the flag.
 *
 * name - The (command line) flag name.
 * arg - The command line argument to process.
 * flag - Pointer to the corresponding integer flag.
 */
Bool GrokIntFlag(const char* name,
                 const char* arg,
                 int* flag);

/*
 * Attempts to parse a char* command line argument.
 * Returns TRUE iff able to parse and set the flag.
 *
 * name - The (command line) flag name.
 * arg - The command line argument to process.
 * flag - Pointer to the corresponding char* flag.
 */
Bool GrokCstringFlag(const char* name,
                     const char* arg,
                     char** flag);

/*
 * Attempts to parse a 32 bit hex valued flag.
 * Returns TRUE iff able to parse and set the flag.
 *
 * name - The (command line) flag name.
 * arg - The command line argument to process.
 * flag - Pointer to the corresponding uint32_t flag.
 */
Bool GrokUint32HexFlag(const char* name,
                       const char* arg,
                       uint32_t* flag);

EXTERN_C_END

#endif  /*  NATIVE_CLIENT_SRC_SHARED_UTILS_FLAGS_H__ */
