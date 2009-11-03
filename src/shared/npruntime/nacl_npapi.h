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


// NaCl-NPAPI Interface

#ifndef NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NACL_NPAPI_H_
#define NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NACL_NPAPI_H_

/**
 * @file
 * Defines the API in the
 * <a href="group___n_p_a_p_i.html">NPAPI Extensions</a>.
 *
 * @addtogroup NPAPI
 * @{
 */

#if NACL_WINDOWS
#include <windows.h>
#endif

#ifdef XP_UNIX
#include <stdio.h>
#endif

#if defined(__native_client__)
/* from sdk */
#include <nacl/npapi.h>
#include <nacl/npruntime.h>
#include <nacl/nacl_srpc.h>
#else
/*
 * from third_party primarily for tests/npapi_bridge/ which build in a
 * non nacl env
 */
#include "native_client/src/third_party/npapi/files/include/npapi.h"
#include "native_client/src/third_party/npapi/files/include/npruntime.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 *  @nacl
 *  Creates a JavaScript array object.
 *  @param npp Pointer to the plug-in instance.
 *  @return Pointer to the new JavaScript array object, or NULL on failure.
 */
NPObject* NaClNPN_CreateArray(NPP npp);

/**
 *  @nacl
 *  Gets a file descriptor for the specified URL. On success, the notify
 *  function will be called back. If the file specified by the URL has been
 *  successfully opened, the handle argument for the notify function will
 *  be set to a valid file descriptor; otherwise, it will be set to -1.
 *  This function enforces cross-domain access restrictions.
 *
 *  Note: NaClNPN_OpenURL() returns an error when another NaClNPN_OpenURL()
 *  operation is in progress in background.
 *  @param npp Pointer to the plug-in instance.
 *  @param url Pointer to the URL of the request.
 *  @param notify_data Plug-in private value for associating the request with
 *         the subsequent notify call.
 *  @param notify Pointer to a callback function to be called upon successful
 *         call to the function.
 *  @return NPERR_NO_ERROR if successful; otherwise, an error code.
 */
NPError NaClNPN_OpenURL(NPP npp, const char* url, void* notify_data,
                        void (*notify)(const char* url, void* notify_data,
                                       NaClSrpcImcDescType handle));

/**
 *  @nacl
 *  Returns a scriptable object pointer that corresponds to
 *  NPPVpluginScriptableNPObject queried by NPP_GetValue().
 *  @param instance Pointer to the plug-in instance.
 *  @return A scriptable object pointer if the module is scriptable, or NULL if
 *          not.
 */
NPObject* NPP_GetScriptableInstance(NPP instance);

#ifdef __cplusplus
}
#endif  // __cplusplus

/**
 * @} End of NPAPI group
 */

#ifdef __cplusplus
namespace nacl {
/*
 *  Undocumented: Prints out an NPIdentifier.
 */
void PrintIdent(NPIdentifier identifier);

/*
 *  Undocumented: Prints out an NPVariant.
 */
void PrintVariant(const NPVariant* variant);
}  // namespace nacl
#endif  // __cplusplus

#endif  // NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NACL_NPAPI_H_
