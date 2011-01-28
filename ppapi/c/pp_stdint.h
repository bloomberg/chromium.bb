/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PP_STDINT_H_
#define PPAPI_C_PP_STDINT_H_

/**
 * @file
 * Provides a definition of C99 sized types
 * across different compilers.
 */
#if defined(_MSC_VER)

typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;

#else
#include <stdint.h>
#endif

#include <stddef.h>  /* Needed for size_t. */

#endif  /* PPAPI_C_PP_STDINT_H_ */

