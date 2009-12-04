/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * NaCl Basic Common Definitions.
 */
#ifndef NATIVE_CLIENT_SRC_INCLUDE_NACL_BASE_H_
#define NATIVE_CLIENT_SRC_INCLUDE_NACL_BASE_H_ 1

/*
 * putting extern "C" { } in header files make emacs want to indent
 * everything, which looks odd.  rather than putting in fancy syntax
 * recognition in c-mode, we just use the following macros.
 *
 * TODO: before releasing code, we should provide a defintion of a
 * function to be called from c-mode-hook that will make it easy to
 * follow our coding style (which we also need to document).
 */
#ifdef __cplusplus
# define EXTERN_C_BEGIN  extern "C" {
# define EXTERN_C_END    }
# if !defined(DISALLOW_COPY_AND_ASSIGN)
/*
 * This code is duplicated from base/basictypes.h, but including
 * that code should not be done except when building as part of Chrome.
 * Removing inclusion was necessitated by the fact that base/basictypes.h
 * sometimes defines CHECK, which conflicts with the NaCl definition.
 * Unfortunately this causes an include order dependency (this file has to
 * come after base/basictypes.h).
 * TODO(sehr): change CHECK to NACL_CHECK everywhere and remove this definition.
 */
#  define DISALLOW_COPY_AND_ASSIGN(TypeName) \
     TypeName(const TypeName&); \
     void operator=(const TypeName&)
#endif  /* !defined(DISALLOW_COPY_AND_ASSIGN) */
#else
# define EXTERN_C_BEGIN
# define EXTERN_C_END
#endif

/*
 * This is necessary to make "#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86" work.
 * #if-directives can work only with numerical values but not with strings e.g.
 * "NACL_x86"; therefore, we convert strings into integers. Whenever you use
 * NACL_ARCH or NACL_arm, you need to include this header.
 */
#define NACL_MERGE(x, y) x ## y
#define NACL_ARCH(x) NACL_MERGE(NACL_, x)
#define NACL_x86  0
#define NACL_arm  1

#endif  /* NATIVE_CLIENT_SRC_INCLUDE_NACL_BASE_H_ */
