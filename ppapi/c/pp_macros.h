// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_PP_MACROS_H_
#define PPAPI_C_PP_MACROS_H_

/**
 * @file
 * Defines the API ...
 *
 * @addtogroup PP
 * @{
 */

/* Use PP_INLINE to tell the compiler to inline functions.  The main purpose of
 * inline functions in ppapi is to allow us to define convenience functions in
 * the ppapi header files, without requiring clients or implementers to link a
 * PPAPI C library.  The "inline" keyword is not supported by pre-C99 C
 * compilers (such as MS Visual Studio 2008 and older versions of GCC).  MSVS
 * supports __forceinline and GCC supports __inline__.  Use of the static
 * keyword ensures (in C) that the function is not compiled on its own, which
 * could cause multiple definition errors.
 *  http://msdn.microsoft.com/en-us/library/z8y1yy88.aspx
 *  http://gcc.gnu.org/onlinedocs/gcc/Inline.html
 */
#if defined(__cplusplus)
/* The inline keyword is part of C++ and guarantees we won't get multiple
 * definition errors.
 */
# define PP_INLINE inline
#else
# if defined(_MSC_VER)
#  define PP_INLINE static __forceinline
# else
#  define PP_INLINE static __inline__
# endif
#endif

/**
 * @}
 * End of addtogroup PP
 */

#endif  // PPAPI_C_PP_MACROS_H_

