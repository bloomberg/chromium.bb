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

#include "platform/wtf/Compiler.h"

/* CPU() - the target CPU architecture */
#define CPU(WTF_FEATURE) \
  (defined WTF_CPU_##WTF_FEATURE && WTF_CPU_##WTF_FEATURE)

/* ==== CPU() - the target CPU architecture ==== */

/* This defines CPU(BIG_ENDIAN) or nothing, as appropriate. */
/* This defines CPU(32BIT) or CPU(64BIT), as appropriate. */

/* CPU(X86) - i386 / x86 32-bit */
#if defined(__i386__) || defined(i386) || defined(_M_IX86) || \
    defined(_X86_) || defined(__THW_INTEL)
#define WTF_CPU_X86 1
#endif

/* CPU(X86_64) - AMD64 / Intel64 / x86_64 64-bit */
#if defined(__x86_64__) || defined(_M_X64)
#define WTF_CPU_X86_64 1
#define WTF_CPU_64BIT 1
#endif

/* CPU(ARM) - ARM, any version*/
#if defined(arm) || defined(__arm__) || defined(ARM) || defined(_ARM_)
#define WTF_CPU_ARM 1

#if defined(__ARMEB__)
#define WTF_CPU_BIG_ENDIAN 1

#elif !defined(__ARM_EABI__) && !defined(__EABI__) && !defined(__VFP_FP__) && \
    !defined(_WIN32_WCE) && !defined(ANDROID)
#define WTF_CPU_MIDDLE_ENDIAN 1

#endif

#if defined(__ARM_NEON__) && !defined(WTF_CPU_ARM_NEON)
#define WTF_CPU_ARM_NEON 1
#endif

#endif /* ARM */

/* CPU(ARM64) - AArch64 64-bit */
#if defined(__aarch64__)
#define WTF_CPU_ARM64 1
#define WTF_CPU_64BIT 1
#endif

/* CPU(MIPS), CPU(MIPS64) */
#if defined(__mips__) && (__mips == 64)
#define WTF_CPU_MIPS64 1
#define WTF_CPU_64BIT 1
#elif defined(__mips__)
#define WTF_CPU_MIPS 1
#endif

#if defined(__mips_msa) && defined(__mips_isa_rev) && (__mips_isa_rev >= 5)
// All MSA intrinsics usage can be disabled by this macro.
#define HAVE_MIPS_MSA_INTRINSICS 1
#endif

#if !defined(WTF_CPU_64BIT)
#define WTF_CPU_32BIT 1
#endif

#endif /* WTF_CPU_h */
