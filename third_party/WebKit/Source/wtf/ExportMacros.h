/*
 * Copyright (C) 2011 Apple Inc.  All rights reserved.
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
 *
 * This file handles shared library symbol export decorations. It is recommended
 * that all WebKit projects use these definitions so that symbol exports work
 * properly on all platforms and compilers that WebKit builds under.
 */

#ifndef ExportMacros_h
#define ExportMacros_h

#include <wtf/Platform.h>

// Different platforms have different defaults for symbol visibility. Usually
// the compiler and the linker just take care of it. However for references to
// runtime routines from JIT stubs, it matters to be able to declare a symbol as
// being local to the target being generated, and thus not subject to (e.g.) ELF
// symbol interposition rules.

#if defined(__GNUC__) && !defined(__CC_ARM) && !defined(__ARMCC__)
#define HAVE_INTERNAL_VISIBILITY 1
#define WTF_INTERNAL __attribute__((visibility("hidden")))
#else
#define WTF_INTERNAL
#endif

#if defined(__GNUC__) && !defined(__CC_ARM) && !defined(__ARMCC__)

#define WTF_EXPORT_DECLARATION __attribute__((visibility("default")))
#define WTF_IMPORT_DECLARATION WTF_EXPORT_DECLARATION
#define WTF_HIDDEN_DECLARATION __attribute__((visibility("hidden")))

#else

#define WTF_EXPORT_DECLARATION
#define WTF_IMPORT_DECLARATION
#define WTF_HIDDEN_DECLARATION

#endif

// FIXME: Remove the following empty macros.
#define WTF_EXPORT_PRIVATE
#define WTF_EXPORT_PRIVATE_RTTI
#define WTF_EXPORT_PRIVATE_NO_RTTI
#define WTF_EXPORT_STRING_API
#define WTF_EXPORT_HIDDEN

#define HIDDEN_INLINE WTF_EXPORT_HIDDEN inline

#endif // ExportMacros_h
