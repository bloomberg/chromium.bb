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


// C/C++ library for handler passing in the Windows Chrome sandbox.
// sel_ldr side interface.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_HANDLE_PASS_LDR_HANDLE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_HANDLE_PASS_LDR_HANDLE_H_

#if NACL_WINDOWS && !defined(NACL_STANDALONE)

#include <windows.h>
#include "native_client/src/include/nacl_base.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"

EXTERN_C_BEGIN

// Initialization function that needs to be called before the constructor.
void NaClHandlePassLdrInit();

// Construct the handle passing client.  Sets the socket address used to
// establish a connection to the lookup server in the browser plugin, and
// creates an SRPC client.  Returns 1 if connection was successful or 0
// otherwise.
extern int NaClHandlePassLdrCtor(struct NaClDesc* socket_address,
                                 DWORD renderer_pid,
                                 NaClHandle renderer_handle);

// Returns the HANDLE associated with the specified PID.  This may be
// returned from a local cache or by querying the lookup server in the
// browser plugin.  Both the current PID and the recipient PID must be
// known to the server for a successful lookup.
extern HANDLE NaClHandlePassLdrLookupHandle(DWORD recipient_pid);

// Destroy the handle passing client.
extern void NaClHandlePassLdrDtor();

EXTERN_C_END

#endif  // NACL_WINDOWS && !defined(NACL_STANDALONE)

#endif  // NATIVE_CLIENT_SRC_TRUSTED_HANDLE_PASS_LDR_HANDLE_H_
