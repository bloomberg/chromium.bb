// Copyright (c) 2008 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
// TODO(sehr): get this from portability rather than defining here.
#ifndef ATTRIBUTE_FORMAT_PRINTF
#define ATTRIBUTE_FORMAT_PRINTF(m, n) __attribute__((format(printf, m, n)))
#endif  // ATTRIBUTE_FORMAT_PRINTF
#else
/*
 * from third_party primarily for tests/npapi_bridge/ which build in a
 * non nacl env
 */
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npruntime.h"
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

/**
 *  All of the following functions have the following usage information:
 *  1) Their result is controlled by the setting of NACL_NPAPI_DEBUG.
 *     If this environment variable is not set, DebugPrintf produces no
 *     output, and the formatters return pointers to zero-length strings.
 *  2) The formatters are not reentrant, as they use static character buffers
 *     to return their output.
 *  3) As a consequence of (2), ownership of the formatted string is not
 *     passed to the caller, and hence free/delete should not be used on
 *     the results.
 *  TODO(sehr): change this interface to be more C++ and use strings.
 */

/**
 *  Undocumented: Prints out a debug string.
 */
void DebugPrintf(const char *fmt, ...) ATTRIBUTE_FORMAT_PRINTF(1, 2);

/**
 *  Undocumented: Formats an NPIdentifier.
 */
const char* FormatNPIdentifier(NPIdentifier ident);

/**
 *  Undocumented: Formats an NPVariant.
 */
const char* FormatNPVariant(const NPVariant* variant);

/**
 *  Undocumented: Formats a vector of NPVariants.
 */
const char* FormatNPVariantVector(const NPVariant* variant, uint32_t count);

}  // namespace nacl
#endif  // __cplusplus

#endif  // NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NACL_NPAPI_H_
