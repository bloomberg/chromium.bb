/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
 * Copyright (C) 2010, 2011 Research In Motion Limited. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WTF_CPU_h
#define WTF_CPU_h

#include "wtf/Compiler.h"

/* CPU() - the target CPU architecture */
#define CPU(WTF_FEATURE) (defined WTF_CPU_##WTF_FEATURE  && WTF_CPU_##WTF_FEATURE)

/* ==== CPU() - the target CPU architecture ==== */

/* This also defines CPU(BIG_ENDIAN) or CPU(MIDDLE_ENDIAN) or neither, as appropriate. */

/* CPU(ALPHA) - DEC Alpha */
#if defined(__alpha__)
#define WTF_CPU_ALPHA 1
#endif

/* CPU(IA64) - Itanium / IA-64 */
#if defined(__ia64__)
#define WTF_CPU_IA64 1
#endif

/* CPU(MIPS) - MIPS 32-bit */
/* Note: Only O32 ABI is tested, so we enable it for O32 ABI for now.  */
#if (defined(mips) || defined(__mips__) || defined(MIPS) || defined(_MIPS_)) \
    && defined(_ABIO32)
#define WTF_CPU_MIPS 1
#if defined(__MIPSEB__)
#define WTF_CPU_BIG_ENDIAN 1
#endif
/* MIPS requires allocators to use aligned memory */
#define WTF_USE_ARENA_ALLOC_ALIGNMENT_INTEGER 1
#endif /* MIPS */

/* CPU(PPC) - PowerPC 32-bit */
#if   defined(__ppc__)     \
    || defined(__PPC__)     \
    || defined(__powerpc__) \
    || defined(__powerpc)   \
    || defined(__POWERPC__) \
    || defined(_M_PPC)      \
    || defined(__PPC)
#define WTF_CPU_PPC 1
#define WTF_CPU_BIG_ENDIAN 1
#endif

/* CPU(PPC64) - PowerPC 64-bit */
#if   defined(__ppc64__) \
    || defined(__PPC64__)
#define WTF_CPU_PPC64 1
#define WTF_CPU_BIG_ENDIAN 1
#endif

/* CPU(SH4) - SuperH SH-4 */
#if defined(__SH4__)
#define WTF_CPU_SH4 1
#endif

/* CPU(SPARC32) - SPARC 32-bit */
#if defined(__sparc) && !defined(__arch64__) || defined(__sparcv8)
#define WTF_CPU_SPARC32 1
#define WTF_CPU_BIG_ENDIAN 1
#endif

/* CPU(SPARC64) - SPARC 64-bit */
#if defined(__sparc__) && defined(__arch64__) || defined (__sparcv9)
#define WTF_CPU_SPARC64 1
#define WTF_CPU_BIG_ENDIAN 1
#endif

/* CPU(SPARC) - any SPARC, true for CPU(SPARC32) and CPU(SPARC64) */
#if CPU(SPARC32) || CPU(SPARC64)
#define WTF_CPU_SPARC 1
#endif

/* CPU(S390X) - S390 64-bit */
#if defined(__s390x__)
#define WTF_CPU_S390X 1
#define WTF_CPU_BIG_ENDIAN 1
#endif

/* CPU(S390) - S390 32-bit */
#if defined(__s390__)
#define WTF_CPU_S390 1
#define WTF_CPU_BIG_ENDIAN 1
#endif

/* CPU(X86) - i386 / x86 32-bit */
#if   defined(__i386__) \
    || defined(i386)     \
    || defined(_M_IX86)  \
    || defined(_X86_)    \
    || defined(__THW_INTEL)
#define WTF_CPU_X86 1
#endif

/* CPU(X86_64) - AMD64 / Intel64 / x86_64 64-bit */
#if   defined(__x86_64__) \
    || defined(_M_X64)
#define WTF_CPU_X86_64 1
#endif

/* CPU(ARM) - ARM, any version*/
#define WTF_ARM_ARCH_AT_LEAST(N) (CPU(ARM) && defined(WTF_ARM_ARCH_VERSION) && WTF_ARM_ARCH_VERSION >= N)

#if   defined(arm) \
    || defined(__arm__) \
    || defined(ARM) \
    || defined(_ARM_)
#define WTF_CPU_ARM 1

#if defined(__ARMEB__)
#define WTF_CPU_BIG_ENDIAN 1

#elif !defined(__ARM_EABI__) \
    && !defined(__EABI__) \
    && !defined(__VFP_FP__) \
    && !defined(_WIN32_WCE) \
    && !defined(ANDROID)
#define WTF_CPU_MIDDLE_ENDIAN 1

#endif

/* Set WTF_ARM_ARCH_VERSION */
#if   defined(__ARM_ARCH_4__) \
    || defined(__ARM_ARCH_4T__) \
    || defined(__MARM_ARMV4__)
#define WTF_ARM_ARCH_VERSION 4

#elif defined(__ARM_ARCH_5__) \
    || defined(__ARM_ARCH_5T__) \
    || defined(__MARM_ARMV5__)
#define WTF_ARM_ARCH_VERSION 5

#elif defined(__ARM_ARCH_5E__) \
    || defined(__ARM_ARCH_5TE__) \
    || defined(__ARM_ARCH_5TEJ__)
#define WTF_ARM_ARCH_VERSION 5
/*ARMv5TE requires allocators to use aligned memory*/
#define WTF_USE_ARENA_ALLOC_ALIGNMENT_INTEGER 1

#elif defined(__ARM_ARCH_6__) \
    || defined(__ARM_ARCH_6J__) \
    || defined(__ARM_ARCH_6K__) \
    || defined(__ARM_ARCH_6Z__) \
    || defined(__ARM_ARCH_6ZK__) \
    || defined(__ARM_ARCH_6T2__) \
    || defined(__ARMV6__)
#define WTF_ARM_ARCH_VERSION 6

#elif defined(__ARM_ARCH_7A__) \
    || defined(__ARM_ARCH_7R__) \
    || defined(__ARM_ARCH_7S__)
#define WTF_ARM_ARCH_VERSION 7

/* MSVC sets _M_ARM */
#elif defined(_M_ARM)
#define WTF_ARM_ARCH_VERSION _M_ARM
#else
#define WTF_ARM_ARCH_VERSION 0

#endif

/* Set WTF_THUMB_ARCH_VERSION */
#if   defined(__ARM_ARCH_4T__)
#define WTF_THUMB_ARCH_VERSION 1

#elif defined(__ARM_ARCH_5T__) \
    || defined(__ARM_ARCH_5TE__) \
    || defined(__ARM_ARCH_5TEJ__)
#define WTF_THUMB_ARCH_VERSION 2

#elif defined(__ARM_ARCH_6J__) \
    || defined(__ARM_ARCH_6K__) \
    || defined(__ARM_ARCH_6Z__) \
    || defined(__ARM_ARCH_6ZK__) \
    || defined(__ARM_ARCH_6M__)
#define WTF_THUMB_ARCH_VERSION 3

#elif defined(__ARM_ARCH_6T2__) \
    || defined(__ARM_ARCH_7__) \
    || defined(__ARM_ARCH_7A__) \
    || defined(__ARM_ARCH_7M__) \
    || defined(__ARM_ARCH_7R__) \
    || defined(__ARM_ARCH_7S__)
#define WTF_THUMB_ARCH_VERSION 4

#else
#define WTF_THUMB_ARCH_VERSION 0
#endif


/* CPU(ARM_TRADITIONAL) - Thumb2 is not available, only traditional ARM (v4 or greater) */
/* CPU(ARM_THUMB2) - Thumb2 instruction set is available */
/* Only one of these will be defined. */
#if !defined(WTF_CPU_ARM_TRADITIONAL) && !defined(WTF_CPU_ARM_THUMB2)
#  if defined(thumb2) || defined(__thumb2__) \
    || ((defined(__thumb) || defined(__thumb__)) && WTF_THUMB_ARCH_VERSION == 4)
#    define WTF_CPU_ARM_TRADITIONAL 0
#    define WTF_CPU_ARM_THUMB2 1
#  elif WTF_ARM_ARCH_AT_LEAST(4)
#    define WTF_CPU_ARM_TRADITIONAL 1
#    define WTF_CPU_ARM_THUMB2 0
#  else
#    error "Not supported ARM architecture"
#  endif
#elif CPU(ARM_TRADITIONAL) && CPU(ARM_THUMB2) /* Sanity Check */
#  error "Cannot use both of WTF_CPU_ARM_TRADITIONAL and WTF_CPU_ARM_THUMB2 platforms"
#endif /* !defined(WTF_CPU_ARM_TRADITIONAL) && !defined(WTF_CPU_ARM_THUMB2) */

#if defined(__ARM_NEON__) && !defined(WTF_CPU_ARM_NEON)
#define WTF_CPU_ARM_NEON 1
#endif

#if CPU(ARM_NEON) && (!COMPILER(GCC) || GCC_VERSION_AT_LEAST(4, 7, 0))
// All NEON intrinsics usage can be disabled by this macro.
#define HAVE_ARM_NEON_INTRINSICS 1
#endif

#if defined(__ARM_ARCH_7S__)
#define WTF_CPU_APPLE_ARMV7S 1
#endif

#endif /* ARM */

#endif /* WTF_CPU_h */
