/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* These weak stubs are provided for bitcode linking,
 * so that they are defined symbols.
 *
 * They will be overridden by the real functions in
 * libgcc_eh / libgcc_s / libcrt_platform
 *
 * for more information on these functions see:
 * http://www.nongnu.org/libunwind/docs.html
 * NOTE: libunwind uses slightly different function names:
 * e.g. _Unwind_DeleteException -> __libunwind_Unwind_DeleteException
 *
 * These two functions are pnacl specific:
 * _Unwind_PNaClSetResult0
 * _Unwind_PNaClSetResult1
 */

#define STUB(sym)   void sym() { }

STUB(_Unwind_DeleteException)
STUB(_Unwind_GetRegionStart)
STUB(_Unwind_GetDataRelBase)
STUB(_Unwind_GetIP)
STUB(_Unwind_GetIPInfo)
STUB(_Unwind_GetTextRelBase)
STUB(_Unwind_GetLanguageSpecificData)
STUB(_Unwind_SetIP)
STUB(_Unwind_PNaClSetResult0)
STUB(_Unwind_PNaClSetResult1)
STUB(_Unwind_RaiseException)
STUB(_Unwind_Resume_or_Rethrow)
STUB(_Unwind_Resume)
STUB(_Unwind_GetCFA)
STUB(_Unwind_Backtrace)

STUB(longjmp)
STUB(setjmp)

/* These should probably not be exposed, but libLLVMJIT.a
 * in the sandboxed translator refers to them directly.
 * TODO: Determine whether these should actually be part of the ABI.
 */
STUB(__deregister_frame)
STUB(__register_frame)
