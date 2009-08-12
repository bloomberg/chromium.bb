/*
 * Copyright 2009, Google Inc.
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
 * NaCl Server Runtime logging code.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_LOG_INTERN_H__
#define NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_LOG_INTERN_H__

#include "native_client/src/include/nacl_base.h"

EXTERN_C_BEGIN

/*
 * The global variable gNaClLogAbortBehavior should only be modified
 * by test code after NaClLogModuleInit() has been called.
 *
 * This variable is needed to make the test infrastructure simpler: on
 * Windows, abort(3) causes the UI to pop up a window
 * (Retry/Abort/Debug/etc), and while for CHECK macros (see
 * nacl_check.h) we probably do normally want that kind of intrusive,
 * in-your-face error reporting, running death tests on our testing
 * infrastructure in a continuous build, continuous test environment
 * cannot tolerate requiring any human interaction.  And since NaCl
 * developers elsewhere will want to be able to run tests, including
 * regedit kludgery to temporarily disable the popup is not a good
 * idea -- even when scoped to the test application by name (need to
 * do this for every death test), it should be feasible to have
 * multiple copies of the svn source tree, and to run at least the
 * small tests in those tree in parallel.
 */
extern void (*gNaClLogAbortBehavior)(void);

EXTERN_C_END

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_LOG_INTERN_H__ */
