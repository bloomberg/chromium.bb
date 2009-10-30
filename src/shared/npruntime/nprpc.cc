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


#include <string>

#include "native_client/src/shared/npruntime/nprpc.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/include/portability_string.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/shared/npruntime/npbridge.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

namespace nacl {

// This file defines all the serializations for the NPAPI complex objects.


// Converts an array of NPVariant into another array of NPVariant that packed
// differently to support different ABIs between the NaCl module and the
// browser plugin. The target NPVariant size is given by peer_npvariant_size.
//
// Currently ConvertNPVariants() supports only 16 byte NPVariant (Win32) to
// 12 byte NPVariant (Linux, OS X, and NaCl) conversion and vice versa.
static void* ConvertNPVariants(const NPVariant* variant,
                               void* target,
                               size_t peer_npvariant_size,
                               size_t arg_count);

// NPVariants are passed in two NaClSrpcArgs.
// The first contains the fixed-size portion.
// The second contains the optional portion (such as the bytes for NPStrings
// valued variants, or capability for NPObjects).
const NPVariant* RpcArg::GetVariant(bool copy_strings) {
  static const NPVariant null = { NPVariantType_Null, {0} };
  NPVariant* variant =
      reinterpret_cast<NPVariant*>(fixed_.Request(sizeof(NPVariant)));
  if (NULL == variant) {
    // There aren't enough bytes to store an NPVariant.
    return &null;
  }
  fixed_.Consume(sizeof(NPVariant));
  if (NPVARIANT_IS_STRING(*variant)) {
    if (0 == NPVARIANT_TO_STRING(*variant).utf8length &&
        NULL == NPVARIANT_TO_STRING(*variant).utf8characters) {
      // Odd, we were passed a NULL string (length == 0 or characters == NULL).
      STRINGN_TO_NPVARIANT(NULL, 0, *variant);
    } else {
      size_t length = NPVARIANT_TO_STRING(*variant).utf8length;
      char* string = reinterpret_cast<char*>(optional_.Request(length));
      if (NULL == string) {
        // Not enough bytes to read string.
        return &null;
      }
      STRINGZ_TO_NPVARIANT(reinterpret_cast<NPUTF8*>(string), *variant);
      optional_.Consume(length);
      if (copy_strings) {
        // Note we cannot use strdup() here since NPN_ReleaseVariantValue() is
        // unhappy with the string allocated by strdup() in some browser
        // versions.
        void* copy = NPN_MemAlloc(length + 1);
        if (NULL == copy) {
          STRINGN_TO_NPVARIANT(NULL, 0, *variant);
        } else {
          memmove(copy, string, length + 1);
          STRINGN_TO_NPVARIANT(static_cast<NPUTF8*>(copy), length, *variant);
        }
      }
    }
  } else if (NPVARIANT_IS_OBJECT(*variant)) {
    NPCapability* capability =
        reinterpret_cast<NPCapability*>(
            optional_.Request(sizeof(NPCapability)));
    if (NULL == capability) {
      // Not enough bytes to get a capability.
      NULL_TO_NPVARIANT(*variant);
    } else {
      NPBridge* bridge = NPBridge::LookupBridge(npp_);
      NPObject* object = bridge->CreateProxy(npp_, *capability);
      if (NULL == object) {
        NULL_TO_NPVARIANT(*variant);
      } else {
        OBJECT_TO_NPVARIANT(object, *variant);
      }
      optional_.Consume(sizeof(NPCapability));
    }
  }
  return variant;
}

bool RpcArg::PutVariant(const NPVariant* variant) {
  NPBridge* bridge = NPBridge::LookupBridge(npp_);
  // Get the size of an NPVariant in the destination.
  size_t peer_npvariant_size = bridge->peer_npvariant_size();
  // First the fixed portions.
  NPVariant* ptr =
      reinterpret_cast<NPVariant*>(fixed_.Request(peer_npvariant_size));
  if (NULL == ptr) {
    // There aren't enough bytes to store an NPVariant.
    return false;
  }
  fixed_.Consume(peer_npvariant_size);
  ConvertNPVariants(variant, ptr, peer_npvariant_size, 1);
  // Aid the optional portions.
  if (NPVARIANT_IS_STRING(*ptr)) {
    if (0 == NPVARIANT_TO_STRING(*ptr).utf8length ||
        NULL == NPVARIANT_TO_STRING(*ptr).utf8characters) {
      // Something's wrong with this string.
      return false;
    } else {
      size_t len = NPVARIANT_TO_STRING(*ptr).utf8length;
      const char* npstr = NPVARIANT_TO_STRING(*ptr).utf8characters;
      void* str = optional_.Request(len);
      if (NULL == str) {
        // There aren't enough bytes to store the string.
        return false;
      }
      optional_.Consume(len);
      memcpy(str, reinterpret_cast<const void*>(npstr), len);
    }
  } else if (NPVARIANT_IS_OBJECT(*variant)) {
    NPObject* object = NPVARIANT_TO_OBJECT(*variant);
    // Passing objects is done by passing a capability.
    NPCapability* capability =
        reinterpret_cast<NPCapability*>(
            optional_.Request(sizeof(NPCapability)));
    if (NULL == capability) {
      return false;
    }
    optional_.Consume(sizeof(NPCapability));
    bridge->CreateStub(npp_, object, capability);
  }
  return true;
}

const NPVariant* RpcArg::GetVariantArray(uint32_t count) {
  if (0 == count) {
    return NULL;
  }
  const NPVariant* variants = GetVariant(true);
  if (NULL == variants) {
    // There were no variants present in the array.
    return NULL;
  }
  for (uint32_t i = 1; i < count; ++i) {
    const NPVariant* next_variant = GetVariant(true);
    if (variants + i != next_variant) {
      // The expectation is that the GetVariant calls mutate the received
      // buffer and return consecutive pointers in that buffer.  If that
      // expection is not satisfied, we return NULL.
      return NULL;
    }
  }
  return variants;
}

bool RpcArg::PutVariantArray(const NPVariant* variants, uint32_t count) {
  uint32_t i;
  for (i = 0; i < count; ++i) {
    if (!PutVariant(variants + i)) {
      return false;
    }
  }
  return true;
}

NPObject* RpcArg::GetObject() {
  NPBridge* bridge = NPBridge::LookupBridge(npp_);
  NPCapability* capability =
      reinterpret_cast<NPCapability*>(fixed_.Request(sizeof(NPCapability)));
  if (NULL == capability) {
    return NULL;
  }
  fixed_.Consume(sizeof(NPCapability));
  return bridge->CreateProxy(npp_, *capability);
}

bool RpcArg::PutObject(NPObject* object) {
  NPBridge* bridge = NPBridge::LookupBridge(npp_);
  NPCapability* capability =
      reinterpret_cast<NPCapability*>(fixed_.Request(sizeof(NPCapability)));
  if (NULL == capability) {
    return false;
  }
  fixed_.Consume(sizeof(NPCapability));
  bridge->CreateStub(npp_, object, capability);
  return true;
}

NPCapability* RpcArg::GetCapability() {
  return reinterpret_cast<NPCapability*>(fixed_.Request(sizeof(NPCapability)));
}

bool RpcArg::PutCapability(const NPCapability* capability) {
  printf("PutCapability %p\n", reinterpret_cast<const void*>(capability));
  NPCapability* ptr =
      reinterpret_cast<NPCapability*>(fixed_.Request(sizeof(NPCapability)));
  printf("    got the space %p\n", reinterpret_cast<void*>(ptr));
  if (NULL == ptr) {
    return false;
  }
  fixed_.Consume(sizeof(NPCapability));
  printf("    consumed the space %p\n", reinterpret_cast<void*>(ptr));
  memcpy(ptr, capability, sizeof(NPCapability));
  printf("    copied %p\n", reinterpret_cast<void*>(ptr));
  return true;
}

NPSize* RpcArg::GetSize() {
  return reinterpret_cast<NPSize*>(fixed_.Request(sizeof(NPSize)));
}

bool RpcArg::PutSize(const NPSize* size) {
  NPSize* ptr = reinterpret_cast<NPSize*>(fixed_.Request(sizeof(NPSize)));
  if (NULL == ptr) {
    return false;
  }
  fixed_.Consume(sizeof(NPSize));
  memcpy(ptr, size, sizeof(NPSize));
  return true;
}

NPRect* RpcArg::GetRect() {
  return reinterpret_cast<NPRect*>(fixed_.Request(sizeof(NPRect)));
}

bool RpcArg::PutRect(const NPRect* rect) {
  NPRect* ptr = reinterpret_cast<NPRect*>(fixed_.Request(sizeof(NPRect)));
  if (NULL == ptr) {
    return false;
  }
  fixed_.Consume(sizeof(NPRect));
  memcpy(ptr, rect, sizeof(NPRect));
  return true;
}

void* ConvertNPVariants(const NPVariant* variant,
                        void* target,
                        size_t peer_npvariant_size,
                        size_t arg_count) {
  // NPVariant is defined in npruntime.h.
  // If peer_npvariant_size is 12, offsetof(NPVariant, value) is 4.
  // If peer_npvariant_size is 16, offsetof(NPVariant, value) is 8.
  if (kParamMax < arg_count) {
    return NULL;
  }
  if (sizeof(NPVariant) == 12 && peer_npvariant_size == 16) {
    while (0 < arg_count--) {
      memmove(target, variant, 4);
      memmove(static_cast<char*>(target) + 8,
              reinterpret_cast<const char*>(variant) + 4,
              8);
      ++variant;
      target = static_cast<char*>(target) + peer_npvariant_size;
    }
  } else if (sizeof(NPVariant) == 16 && peer_npvariant_size == 12) {
    while (0 < arg_count--) {
      memmove(target, variant, 4);
      memmove(static_cast<char*>(target) + 4,
              reinterpret_cast<const char*>(variant) + 8,
              8);
      ++variant;
      target = static_cast<char*>(target) + peer_npvariant_size;
    }
  } else if (sizeof(NPVariant) == peer_npvariant_size) {
    memmove(target, variant, arg_count * peer_npvariant_size);
    target = static_cast<char*>(target) + arg_count * peer_npvariant_size;
  } else {
    fprintf(stderr, "convertNPVariant: unexpected NPVariant size.\n");
    exit(EXIT_FAILURE);
  }
  return target;
}

}  // namespace nacl
