// Copyright (c) 2008 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NaCl-NPAPI Interface

#ifndef NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NACL_NPAPI_H_
#define NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NACL_NPAPI_H_

#if defined(__native_client__)
/* from sdk */
#include <nacl/npapi.h>
#include <nacl/npruntime.h>
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
#include "native_client/src/include/portability.h"
#endif

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
