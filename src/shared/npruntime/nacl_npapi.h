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
#ifdef __cplusplus
#include <nacl/nacl_htp.h>
#else
#include <nacl/nacl_htp_c.h>
#endif  // __cplusplus
#else
/*
 * from third_party primarily for tests/npapi_bridge/ which build in a
 * non nacl env
 */
#include "native_client/src/third_party/npapi/files/include/npapi.h"
#include "native_client/src/third_party/npapi/files/include/npruntime.h"
#ifdef __cplusplus
#include "native_client/src/shared/imc/nacl_htp.h"
#else
#include "native_client/src/shared/imc/nacl_htp_c.h"
#endif  // __cplusplus
#endif

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 *  @nacl
 *  Initializes the NPAPI bridge RPC runtime.
 *  @param argc Pointer to the program's unmodified argc variable from main().
 *  @param argv The program's unmodified argv variable from main().
 *  @return 0 upon success; otherwise, -1.
 */
int NaClNP_Init(int* argc, char* argv[]);

/**
 *  @nacl
 *  Enters the NPAPI bridge RPC processing loop. NaClNP_MainLoop() should be
 *  called once after NaClNP_Init() is called successfully.
 *  @param flags Reserved. Use 0.
 *  @return 0 upon success; otherwise, -1.
 */
int NaClNP_MainLoop(unsigned flags);

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
#ifdef __cplusplus
NPError NaClNPN_OpenURL(NPP npp, const char* url, void* notify_data,
                        void (*notify)(const char* url, void* notify_data,
                                       nacl::HtpHandle handle));
#else   // __cplusplus
#ifdef __native_client__
NPError NaClNPN_OpenURL(NPP npp, const char* url, void* notify_data,
                        void (*notify)(const char* url, void* notify_data,
                                       int handle));
#else   // __native_client__
NPError NaClNPN_OpenURL(NPP npp, const char* url, void* notify_data,
                        void (*notify)(const char* url, void* notify_data,
                                       struct NaClDesc* handle));
#endif  // __native_client__
#endif  // __cplusplus

/**
 *  @nacl
 *  Returns a scriptable object pointer that corresponds to
 *  NPPVpluginScriptableNPObject queried by NPP_GetValue().
 *  @param instance Pointer to the plug-in instance.
 *  @return A scriptable object pointer if the module is scriptable, or NULL if
 *          not.
 */
NPObject* NPP_GetScriptableInstance(NPP instance);

enum {
  NPVariantType_Handle = 127
};

/**
 *  @nacl
 *  Checks whether the NPVariant holds a NaCl resource descriptor.
 *  @param _v The NPVariant value.
 *  @return Non-zero value if the NPVariant holds a NaCl resource descriptor;
 *          otherwise, 0.
 */
#define NPVARIANT_IS_HANDLE(_v) \
    ((int) (_v).type == (int) NPVariantType_Handle)

#ifdef __native_client__

/**
 *  @nacl
 *  Gets a NaCl resource descriptor from an NPVariant.
 *  @param _v The NPVariant value.
 *  @return The NaCl resource descriptor.
 */
#define NPVARIANT_TO_HANDLE(_v)  ((_v).value.intValue)

#else  // __native_client__

#ifdef __cplusplus

#define NPVARIANT_TO_HANDLE(_v) \
    reinterpret_cast<nacl::HtpHandle>((_v).value.objectValue)

#else  // __cplusplus

#define NPVARIANT_TO_HANDLE(_v) \
    (NaClHtpHandle) ((_v).value.objectValue)

#endif  // __cplusplus

#endif  // __native_client__

#ifdef __native_client__

/**
 *  @nacl
 *  Sets a NaCl resource descriptor to an NPVariant.
 *  @param _val The NaCl resource descriptor to set.
 *  @param _v The NPVariant value.
 */
#define HANDLE_TO_NPVARIANT(_val, _v) \
    NP_BEGIN_MACRO \
    (_v).type = (NPVariantType) NPVariantType_Handle; \
    (_v).value.intValue = _val; \
    NP_END_MACRO

#else  // __native_client__

#define HANDLE_TO_NPVARIANT(_val, _v) \
    NP_BEGIN_MACRO \
    (_v).type = (NPVariantType) NPVariantType_Handle; \
    (_v).value.objectValue = (NPObject*) (_val); \
    NP_END_MACRO

#endif  // __native_client__

#ifdef __cplusplus
}
#endif  // __cplusplus

/**
 * @} End of NPAPI group
 */

#endif  // NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NACL_NPAPI_H_
