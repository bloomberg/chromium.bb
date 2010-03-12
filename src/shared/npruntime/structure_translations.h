// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NaCl-NPAPI structure serialization/deserialization.

#ifndef NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPRPC_H_
#define NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPRPC_H_

#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/shared/npruntime/npcapability.h"

namespace nacl {

// The maximum size value for serialization.
const size_t kNaClAbiSizeTMax =
    static_cast<size_t>(~static_cast<nacl_abi_size_t>(0));

/*
 * Serializes a vector of NPVariants into a buffer.  The client must pass
 * the maximum length to be serialized.  The client may also pass a buffer
 * to serialize into.  If the bytes pointer is NULL, memory is allocated for
 * the buffer.
 * If successful, *length contains the number of bytes used to serialize,
 * and the buffer address is returned.  Otherwise, it returns NULL.
 */
char* NPVariantsToWireFormat(NPP npp,
                             const NPVariant* variants,
                             uint32_t argc,
                             char* bytes,
                             nacl_abi_size_t* length);

/*
 * Deserializes a buffer into a vector of NPVariants. The client may pass
 * the address of a vector of NPVariants to serialize into.  If the variants
 * pointer is NULL, memory is allocated for the vector of variants.
 * If successful, the address of the vector of variants is returned.
 * Otherwise, it returns NULL.
 */
NPVariant* WireFormatToNPVariants(NPP npp,
                                  char* bytes,
                                  nacl_abi_size_t length,
                                  uint32_t argc,
                                  NPVariant* variants);

/*
 * The maximum allowed size of a serialized NPVariant structure in bytes on
 * all platforms.  This is needed today because the memory for return values
 * in SRPC is allocated by the caller.
 */
const size_t kNPVariantSizeMax = 16 * 1024;

/*
 * Serializes an NPCapability into a buffer.  The client must pass
 * the maximum length to be serialized.  The client may also pass a buffer
 * to serialize into.  If the bytes pointer is NULL, memory is allocated for
 * the buffer.
 * If successful, *length contains the number of bytes used to serialize,
 * and the buffer address is returned.  Otherwise, it returns NULL.
 */
char* NPCapabilityToWireFormat(NPCapability* capability,
                               char* bytes,
                               nacl_abi_size_t* length);

/*
 * Deserializes a buffer into an NPCapability. The client may pass
 * the address of an NPCapability to serialize into.  If the capability
 * pointer is NULL, memory is allocated for the capability.
 * If successful, the address of the capability is returned.
 * Otherwise, it returns NULL.
 */
NPCapability* WireFormatToNPCapability(char* bytes,
                                       nacl_abi_size_t length,
                                       NPCapability* capability);

/*
 * Serializes an NPObject into a buffer.  The client must pass
 * the maximum length to be serialized.  The client may also pass a buffer
 * to serialize into.  If the bytes pointer is NULL, memory is allocated for
 * the buffer.
 * If successful, *length contains the number of bytes used to serialize,
 * and the buffer address is returned.  Otherwise, it returns NULL.
 */
char* NPObjectToWireFormat(NPP npp,
                           NPObject* object,
                           char* bytes,
                           nacl_abi_size_t* length);

/*
 * Deserializes a buffer into an NPObject.
 * If successful, the address of the NPObject is returned.
 * Otherwise, it returns NULL.
 */
NPObject* WireFormatToNPObject(NPP npp,
                               char* bytes,
                               nacl_abi_size_t length);

/*
 * Serializes an NPString into a buffer.  The client must pass
 * the maximum length to be serialized.  The client may also pass a buffer
 * to serialize into.  If the bytes pointer is NULL, memory is allocated for
 * the buffer.
 * If successful, *length contains the number of bytes used to serialize,
 * and the buffer address is returned.  Otherwise, it returns NULL.
 */
char* NPStringToWireFormat(NPString* string,
                           char* bytes,
                           nacl_abi_size_t* length);

/*
 * Deserializes a buffer into an NPString. The client may pass
 * the address of an NPString to serialize into.  If the str
 * pointer is NULL, memory is allocated for the string.
 * If successful, the address of the NPString is returned.
 * Otherwise, it returns NULL.
 */
NPString* WireFormatToNPString(char* bytes,
                               nacl_abi_size_t length,
                               NPString* str);

}  // namespace nacl

#endif  // NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPRPC_H_
