// Copyright (c) 2012, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef GOOGLE_BREAKPAD_COMMON_ANDROID_INCLUDE_SYS_USER_H
#define GOOGLE_BREAKPAD_COMMON_ANDROID_INCLUDE_SYS_USER_H

// The purpose of this file is to glue the mismatching headers (Android NDK vs
// glibc) and therefore avoid doing otherwise awkward #ifdefs in the code.
// The following quirks are currently handled by this file:
// - i386: Use the Android NDK but alias user_fxsr_struct > user_fpxregs_struct.
// - x86_64: Override a typo in user_fpregs_struct (mxcsr_mask -> mxcr_mask).
//     The typo has been fixed in NDK r10d, but a preprocessor workaround is
//     required to make breakpad build with r10c and lower (more details below).
// - Other platforms: Just use the Android NDK unchanged.

// TODO(primiano): remove this after Chromium has stably rolled to NDK r10d.
// Historical context: NDK releases < r10d had a typo in sys/user.h (mxcsr_mask
// instead of mxcr_mask), which is fixed in r10d. However, just switching to use
// the correct one (mxcr_mask) would put Breakpad in a state where it can be
// rolled in chromium only atomically with the r10d NDK. A revert of either
// project (android_tools, breakpad) would make the other one unrollable.
// This hack makes breakpad code compatible with both r10c and r10d NDKs,
// reducing the dependency entangling with android_tools.
#if defined(__x86_64__)
#define mxcsr_mask mxcr_mask
#endif

#include_next <sys/user.h>

#if defined(__x86_64__)
#undef mxcsr_mask
#endif

#ifdef __i386__
#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus
typedef struct user_fxsr_struct user_fpxregs_struct;
#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // __i386__

#endif  // GOOGLE_BREAKPAD_COMMON_ANDROID_INCLUDE_SYS_USER_H
