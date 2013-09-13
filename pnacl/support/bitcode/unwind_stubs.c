/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* The PNaCl stable ABI does not support C++ exceptions and nexes are not
 * linked with libgcc_eh.a which provides a number of _Unwind_* functions.
 * Some parts of the pexe (e.g. libstdc++) refer to these functions
 * so we provide abort()-ing stubs here. It gets linked into pexes
 * during a stable ABI build.
 */

#include <stdlib.h>
#include <unistd.h>

#define STUB(sym) \
  void sym() { \
    char msg[] = "Aborting: " #sym " called " \
                 "(C++ exception handling is disabled)\n"; \
    write(2, msg, sizeof(msg) - 1); \
    abort(); \
  }

STUB(_Unwind_DeleteException)
STUB(_Unwind_GetRegionStart)
STUB(_Unwind_GetDataRelBase)
STUB(_Unwind_GetGR)
STUB(_Unwind_GetIP)
STUB(_Unwind_GetIPInfo)
STUB(_Unwind_GetTextRelBase)
STUB(_Unwind_GetLanguageSpecificData)
STUB(_Unwind_SetGR)
STUB(_Unwind_SetIP)
STUB(_Unwind_PNaClSetResult0)
STUB(_Unwind_PNaClSetResult1)
STUB(_Unwind_RaiseException)
STUB(_Unwind_Resume_or_Rethrow)
STUB(_Unwind_Resume)
STUB(_Unwind_GetCFA)
STUB(_Unwind_Backtrace)
