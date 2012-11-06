/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
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
