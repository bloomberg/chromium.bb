/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
 * Copyright (C) 2010, 2011 Research In Motion Limited. All rights reserved.
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

#ifndef WTF_Platform_h
#define WTF_Platform_h

/* Include compiler specific macros */
#include <wtf/Compiler.h>

/* ==== PLATFORM handles OS, operating environment, graphics API, and
   CPU. This macro will be phased out in favor of platform adaptation
   macros, policy decision macros, and top-level port definitions. ==== */
#define PLATFORM(WTF_FEATURE) (defined WTF_PLATFORM_##WTF_FEATURE  && WTF_PLATFORM_##WTF_FEATURE)


/* ==== Platform adaptation macros: these describe properties of the target environment. ==== */

/* CPU() - the target CPU architecture */
#define CPU(WTF_FEATURE) (defined WTF_CPU_##WTF_FEATURE  && WTF_CPU_##WTF_FEATURE)
/* HAVE() - specific system features (headers, functions or similar) that are present or not */
#define HAVE(WTF_FEATURE) (defined HAVE_##WTF_FEATURE  && HAVE_##WTF_FEATURE)
/* OS() - underlying operating system; only to be used for mandated low-level services like 
   virtual memory, not to choose a GUI toolkit */
#define OS(WTF_FEATURE) (defined WTF_OS_##WTF_FEATURE  && WTF_OS_##WTF_FEATURE)


/* ==== Policy decision macros: these define policy choices for a particular port. ==== */

/* USE() - use a particular third-party library or optional OS service */
#define USE(WTF_FEATURE) (defined WTF_USE_##WTF_FEATURE  && WTF_USE_##WTF_FEATURE)
/* ENABLE() - turn on a specific feature of WebKit */
#define ENABLE(WTF_FEATURE) (defined ENABLE_##WTF_FEATURE  && ENABLE_##WTF_FEATURE)


/* ==== CPU() - the target CPU architecture ==== */

/* This also defines CPU(BIG_ENDIAN) or CPU(MIDDLE_ENDIAN) or neither, as appropriate. */

/* CPU(ALPHA) - DEC Alpha */
#if defined(__alpha__)
#define WTF_CPU_ALPHA 1
#endif

/* CPU(IA64) - Itanium / IA-64 */
#if defined(__ia64__)
#define WTF_CPU_IA64 1
/* 32-bit mode on Itanium */
#if !defined(__LP64__)
#define WTF_CPU_IA64_32 1
#endif
#endif

/* CPU(MIPS) - MIPS 32-bit */
/* Note: Only O32 ABI is tested, so we enable it for O32 ABI for now.  */
#if (defined(mips) || defined(__mips__) || defined(MIPS) || defined(_MIPS_)) \
    && defined(_ABIO32)
#define WTF_CPU_MIPS 1
#if defined(__MIPSEB__)
#define WTF_CPU_BIG_ENDIAN 1
#endif
#define WTF_MIPS_PIC (defined __PIC__)
#define WTF_MIPS_ARCH __mips
#define WTF_MIPS_ISA(v) (defined WTF_MIPS_ARCH && WTF_MIPS_ARCH == v)
#define WTF_MIPS_ISA_AT_LEAST(v) (defined WTF_MIPS_ARCH && WTF_MIPS_ARCH >= v)
#define WTF_MIPS_ARCH_REV __mips_isa_rev
#define WTF_MIPS_ISA_REV(v) (defined WTF_MIPS_ARCH_REV && WTF_MIPS_ARCH_REV == v)
#define WTF_MIPS_DOUBLE_FLOAT (defined __mips_hard_float && !defined __mips_single_float)
#define WTF_MIPS_FP64 (defined __mips_fpr && __mips_fpr == 64)
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

#if defined(__ARM_PCS_VFP)
#define WTF_CPU_ARM_HARDFP 1
#endif

#if defined(__ARMEB__) || (COMPILER(RVCT) && defined(__BIG_ENDIAN))
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

/* RVCT sets _TARGET_ARCH_ARM */
#elif defined(__TARGET_ARCH_ARM)
#define WTF_ARM_ARCH_VERSION __TARGET_ARCH_ARM

#if defined(__TARGET_ARCH_5E) \
    || defined(__TARGET_ARCH_5TE) \
    || defined(__TARGET_ARCH_5TEJ)
/*ARMv5TE requires allocators to use aligned memory*/
#define WTF_USE_ARENA_ALLOC_ALIGNMENT_INTEGER 1
#endif

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

/* RVCT sets __TARGET_ARCH_THUMB */
#elif defined(__TARGET_ARCH_THUMB)
#define WTF_THUMB_ARCH_VERSION __TARGET_ARCH_THUMB

#else
#define WTF_THUMB_ARCH_VERSION 0
#endif


/* CPU(ARMV5_OR_LOWER) - ARM instruction set v5 or earlier */
/* On ARMv5 and below the natural alignment is required. 
   And there are some other differences for v5 or earlier. */
#if !defined(ARMV5_OR_LOWER) && !WTF_ARM_ARCH_AT_LEAST(6)
#define WTF_CPU_ARMV5_OR_LOWER 1
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

#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
#define WTF_CPU_ARM_VFP 1
#endif

#if defined(__ARM_ARCH_7S__)
#define WTF_CPU_APPLE_ARMV7S 1
#endif

#endif /* ARM */

#if CPU(ARM) || CPU(MIPS) || CPU(SH4) || CPU(SPARC)
#define WTF_CPU_NEEDS_ALIGNED_ACCESS 1
#endif

/* ==== OS() - underlying operating system; only to be used for mandated low-level services like 
   virtual memory, not to choose a GUI toolkit ==== */

/* OS(ANDROID) - Android */
#ifdef ANDROID
#define WTF_OS_ANDROID 1
#endif

/* OS(AIX) - AIX */
#ifdef _AIX
#define WTF_OS_AIX 1
#endif

/* OS(DARWIN) - Any Darwin-based OS, including Mac OS X and iPhone OS */
#ifdef __APPLE__
#define WTF_OS_DARWIN 1

#include <Availability.h>
#include <AvailabilityMacros.h>
#include <TargetConditionals.h>
#endif

/* OS(IOS) - iOS */
/* OS(MAC_OS_X) - Mac OS X (not including iOS) */
#if OS(DARWIN) && ((defined(TARGET_OS_EMBEDDED) && TARGET_OS_EMBEDDED) \
    || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)                 \
    || (defined(TARGET_IPHONE_SIMULATOR) && TARGET_IPHONE_SIMULATOR))
#define WTF_OS_IOS 1
#elif OS(DARWIN) && defined(TARGET_OS_MAC) && TARGET_OS_MAC
#define WTF_OS_MAC_OS_X 1

/* FIXME: These can be removed after sufficient time has passed since the removal of BUILDING_ON / TARGETING macros. */

#define ERROR_PLEASE_COMPARE_WITH_MAC_OS_X_VERSION_MIN_REQUIRED 0 / 0
#define ERROR_PLEASE_COMPARE_WITH_MAC_OS_X_VERSION_MAX_ALLOWED 0 / 0

#define BUILDING_ON_LEOPARD ERROR_PLEASE_COMPARE_WITH_MAC_OS_X_VERSION_MIN_REQUIRED
#define BUILDING_ON_SNOW_LEOPARD ERROR_PLEASE_COMPARE_WITH_MAC_OS_X_VERSION_MIN_REQUIRED
#define BUILDING_ON_LION ERROR_PLEASE_COMPARE_WITH_MAC_OS_X_VERSION_MIN_REQUIRED

#define TARGETING_LEOPARD ERROR_PLEASE_COMPARE_WITH_MAC_OS_X_VERSION_MAX_ALLOWED
#define TARGETING_SNOW_LEOPARD ERROR_PLEASE_COMPARE_WITH_MAC_OS_X_VERSION_MAX_ALLOWED
#define TARGETING_LION ERROR_PLEASE_COMPARE_WITH_MAC_OS_X_VERSION_MAX_ALLOWED
#endif

/* OS(FREEBSD) - FreeBSD */
#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__FreeBSD_kernel__)
#define WTF_OS_FREEBSD 1
#endif

/* OS(HURD) - GNU/Hurd */
#ifdef __GNU__
#define WTF_OS_HURD 1
#endif

/* OS(LINUX) - Linux */
#ifdef __linux__
#define WTF_OS_LINUX 1
#endif

/* OS(NETBSD) - NetBSD */
#if defined(__NetBSD__)
#define WTF_OS_NETBSD 1
#endif

/* OS(OPENBSD) - OpenBSD */
#ifdef __OpenBSD__
#define WTF_OS_OPENBSD 1
#endif

/* OS(QNX) - QNX */
#if defined(__QNXNTO__)
#define WTF_OS_QNX 1
#endif

/* OS(SOLARIS) - Solaris */
#if defined(sun) || defined(__sun)
#define WTF_OS_SOLARIS 1
#endif

/* OS(WINCE) - Windows CE; note that for this platform OS(WINDOWS) is also defined */
#if defined(_WIN32_WCE)
#define WTF_OS_WINCE 1
#endif

/* OS(WINDOWS) - Any version of Windows */
#if defined(WIN32) || defined(_WIN32)
#define WTF_OS_WINDOWS 1
#endif

#define WTF_OS_WIN ERROR "USE WINDOWS WITH OS NOT WIN"
#define WTF_OS_MAC ERROR "USE MAC_OS_X WITH OS NOT MAC"

/* OS(UNIX) - Any Unix-like system */
#if   OS(AIX)              \
    || OS(ANDROID)          \
    || OS(DARWIN)           \
    || OS(FREEBSD)          \
    || OS(HURD)             \
    || OS(LINUX)            \
    || OS(NETBSD)           \
    || OS(OPENBSD)          \
    || OS(QNX)              \
    || OS(SOLARIS)          \
    || defined(unix)        \
    || defined(__unix)      \
    || defined(__unix__)
#define WTF_OS_UNIX 1
#endif

/* Operating environments */

/* FIXME: This should go away since we're only building chromium now */
#define WTF_PLATFORM_CHROMIUM 1

/* Graphics engines */

/* USE(SKIA) for Win/Linux/Mac/Android */
#if OS(DARWIN)
#define WTF_USE_SKIA 1
#define WTF_USE_ICCJPEG 1
#define WTF_USE_QCMSLIB 1
#elif OS(ANDROID)
#define WTF_USE_SKIA 1
#define WTF_USE_LOW_QUALITY_IMAGE_INTERPOLATION 1
#define WTF_USE_LOW_QUALITY_IMAGE_NO_JPEG_DITHERING 1
#define WTF_USE_LOW_QUALITY_IMAGE_NO_JPEG_FANCY_UPSAMPLING 1
#else
#define WTF_USE_SKIA 1
#define WTF_USE_ICCJPEG 1
#define WTF_USE_QCMSLIB 1
#endif

/* On Windows, use QueryPerformanceCounter by default */
#if OS(WINDOWS)
#define WTF_USE_QUERY_PERFORMANCE_COUNTER  1
#endif

#define WTF_USE_ICU_UNICODE 1

#if OS(DARWIN)
#define WTF_USE_CF 1
#define WTF_USE_WK_SCROLLBAR_PAINTER 1

/* We can't override the global operator new and delete on OS(DARWIN) because
 * some object are allocated by WebKit and deallocated by the embedder. */
#else /* !OS(DARWIN) */
/* On non-OS(DARWIN), the "system malloc" is actually TCMalloc anyway, so there's
 * no need to use WebKit's copy of TCMalloc. */
#define WTF_USE_SYSTEM_MALLOC 1
#endif /* OS(DARWIN) */

#if OS(DARWIN)
#define ENABLE_PURGEABLE_MEMORY 1
#endif /* OS(DARWIN) */

#if !defined(HAVE_ACCESSIBILITY)
#if !OS(ANDROID)
#define HAVE_ACCESSIBILITY 1
#endif
#endif /* !defined(HAVE_ACCESSIBILITY) */

#if OS(UNIX)
#define HAVE_ERRNO_H 1
#define HAVE_MMAP 1   
#define HAVE_SIGNAL_H 1
#define HAVE_STRINGS_H 1
#define HAVE_SYS_PARAM_H 1
#define HAVE_SYS_TIME_H 1 
#define WTF_USE_OS_RANDOMNESS 1
#define WTF_USE_PTHREADS 1
#endif /* OS(UNIX) */

#if OS(UNIX) && !OS(ANDROID)
#define HAVE_LANGINFO_H 1
#endif

#if (OS(FREEBSD) || OS(OPENBSD)) && !defined(__GLIBC__)
#define HAVE_PTHREAD_NP_H 1
#endif

#if !defined(HAVE_VASPRINTF)
#if !COMPILER(MSVC) && !COMPILER(RVCT) && !COMPILER(MINGW) && !(COMPILER(GCC) && OS(QNX))
#define HAVE_VASPRINTF 1
#endif
#endif

#if !defined(HAVE_STRNSTR)
#if OS(DARWIN) || (OS(FREEBSD) && !defined(__GLIBC__))
#define HAVE_STRNSTR 1
#endif
#endif

#if !OS(WINDOWS) && !OS(SOLARIS) && !OS(ANDROID)
#define HAVE_TM_GMTOFF 1
#define HAVE_TM_ZONE 1
#define HAVE_TIMEGM 1
#endif

#if OS(DARWIN)

#define HAVE_MERGESORT 1
#define HAVE_SYS_TIMEB_H 1
#define WTF_USE_ACCELERATE 1

#define HAVE_DISPATCH_H 1
#define HAVE_MADV_FREE 1
#define HAVE_PTHREAD_SETNAME_NP 1

#define HAVE_HOSTED_CORE_ANIMATION 1
#define HAVE_MADV_FREE_REUSE 1

#endif /* OS(DARWIN) */

#if OS(WINDOWS)
#define HAVE_SYS_TIMEB_H 1
#define HAVE_ALIGNED_MALLOC 1
#define HAVE_ISDEBUGGERPRESENT 1
#define HAVE_VIRTUALALLOC 1
#define WTF_USE_OS_RANDOMNESS 1
#endif

/* ENABLE macro defaults */

/* FIXME: move out all ENABLE() defines from here to FeatureDefines.h */

/* Include feature macros */
#include <wtf/FeatureDefines.h>

#define ENABLE_DEBUG_WITH_BREAKPOINT 0
#define ENABLE_SAMPLING_COUNTERS 0
#define ENABLE_SAMPLING_FLAGS 0
#define ENABLE_SAMPLING_REGIONS 0
#define ENABLE_OPCODE_SAMPLING 0
#define ENABLE_CODEBLOCK_SAMPLING 0
#if ENABLE(CODEBLOCK_SAMPLING) && !ENABLE(OPCODE_SAMPLING)
#error "CODEBLOCK_SAMPLING requires OPCODE_SAMPLING"
#endif
#if ENABLE(OPCODE_SAMPLING) || ENABLE(SAMPLING_FLAGS) || ENABLE(SAMPLING_REGIONS)
#define ENABLE_SAMPLING_THREAD 1
#endif

#if !defined(WTF_USE_JSVALUE64) && !defined(WTF_USE_JSVALUE32_64)
#if (CPU(X86_64) && (OS(UNIX) || OS(WINDOWS))) \
    || (CPU(IA64) && !CPU(IA64_32)) \
    || CPU(ALPHA) \
    || CPU(SPARC64) \
    || CPU(S390X) \
    || CPU(PPC64)
#define WTF_USE_JSVALUE64 1
#else
#define WTF_USE_JSVALUE32_64 1
#endif
#endif /* !defined(WTF_USE_JSVALUE64) && !defined(WTF_USE_JSVALUE32_64) */

/* Disable the JIT on versions of GCC prior to 4.1 */
#if !defined(ENABLE_JIT) && COMPILER(GCC) && !GCC_VERSION_AT_LEAST(4, 1, 0)
#define ENABLE_JIT 0
#endif

#if !defined(ENABLE_JIT) && CPU(SH4) && PLATFORM(QT)
#define ENABLE_JIT 1
#endif

/* The JIT is enabled by default on all x86, x86-64, ARM & MIPS platforms. */
#if !defined(ENABLE_JIT) \
    && (CPU(X86) || CPU(X86_64) || CPU(ARM) || CPU(MIPS)) \
    && (OS(DARWIN) || !COMPILER(GCC) || GCC_VERSION_AT_LEAST(4, 1, 0)) \
    && !OS(WINCE) \
    && !(OS(QNX) && !PLATFORM(QT)) /* We use JIT in QNX Qt */
#define ENABLE_JIT 1
#endif

/* FIXME: When all platforms' compositors can compute their own filter outsets, we should remove this define. 
   https://bugs.webkit.org/show_bug.cgi?id=112830 */
#if USE(CG)
#define HAVE_COMPOSITOR_FILTER_OUTSETS 1
#endif

#if !defined(WTF_USE_3D_GRAPHICS)
#define WTF_USE_3D_GRAPHICS 1
#endif

#if CPU(ARM_THUMB2)
#define ENABLE_BRANCH_COMPACTION 1
#endif

#if !defined(ENABLE_THREADING_LIBDISPATCH) && HAVE(DISPATCH_H)
#define ENABLE_THREADING_LIBDISPATCH 1
#elif !defined(ENABLE_THREADING_OPENMP) && defined(_OPENMP)
#define ENABLE_THREADING_OPENMP 1
#elif !defined(THREADING_GENERIC)
#define ENABLE_THREADING_GENERIC 1
#endif

#if USE(GLIB)
#include <wtf/gobject/GTypedefs.h>
#endif

/* FIXME: This define won't be needed once #27551 is fully landed. However, 
   since most ports try to support sub-project independence, adding new headers
   to WTF causes many ports to break, and so this way we can address the build
   breakages one port at a time. */
#if !defined(WTF_USE_EXPORT_MACROS) && (PLATFORM(MAC) || PLATFORM(QT) || PLATFORM(WX))
#define WTF_USE_EXPORT_MACROS 1
#endif

#if !defined(WTF_USE_EXPORT_MACROS_FOR_TESTING) && (PLATFORM(GTK) || PLATFORM(WIN))
#define WTF_USE_EXPORT_MACROS_FOR_TESTING 1
#endif

#if (PLATFORM(QT) && !OS(DARWIN) && !OS(WINDOWS)) || PLATFORM(GTK)
#define WTF_USE_UNIX_DOMAIN_SOCKETS 1
#endif

#if !defined(ENABLE_COMPARE_AND_SWAP) && (OS(WINDOWS) || (COMPILER(GCC) && (CPU(X86) || CPU(X86_64) || CPU(ARM_THUMB2))))
#define ENABLE_COMPARE_AND_SWAP 1
#endif

#define ENABLE_OBJECT_MARK_LOGGING 0

#if !defined(ENABLE_PARALLEL_GC) && !ENABLE(OBJECT_MARK_LOGGING) && (PLATFORM(MAC) || PLATFORM(IOS) || PLATFORM(GTK)) && ENABLE(COMPARE_AND_SWAP)
#define ENABLE_PARALLEL_GC 1
#elif PLATFORM(QT)
// Parallel GC is temporarily disabled on Qt because of regular crashes, see https://bugs.webkit.org/show_bug.cgi?id=90957 for details
#define ENABLE_PARALLEL_GC 0
#endif

#if !defined(ENABLE_GC_VALIDATION) && !defined(NDEBUG)
#define ENABLE_GC_VALIDATION 1
#endif

#if !defined(WTF_USE_ZLIB)
#define WTF_USE_ZLIB 1
#endif

#endif /* WTF_Platform_h */
