/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PP_STDINT_H_
#define PPAPI_C_PP_STDINT_H_

/**
 * @file
 * This file provides a definition of C99 sized types
 * for Microsoft compilers. These definitions only apply
 * for trusted modules.
 */

/**
 *
 * @addtogroup Typedefs
 * @{
 */
#if defined(_MSC_VER)

/** This value represents a guaranteed unsigned 8 bit integer. */
typedef unsigned char uint8_t;

/** This value represents a guaranteed signed 8 bit integer. */
typedef signed char int8_t;

/** This value represents a guaranteed unsigned 16 bit short. */
typedef unsigned short uint16_t;

/** This value represents a guaranteed signed 16 bit short. */
typedef short int16_t;

/** This value represents a guaranteed unsigned 32 bit integer. */
typedef unsigned int uint32_t;

/** This value represents a guaranteed signed 32 bit integer. */
typedef int int32_t;

/** This value represents a guaranteed signed 64 bit integer. */
typedef __int64 int64_t;

/** This value represents a guaranteed unsigned 64 bit integer. */
typedef unsigned __int64 uint64_t;
/**
 * @}
 */
#else
#include <stdint.h>
#endif

#include <stddef.h>  /* Needed for size_t. */

#endif  /* PPAPI_C_PP_STDINT_H_ */

