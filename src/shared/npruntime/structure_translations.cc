// Copyright (c) 2008 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/npruntime/structure_translations.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/include/portability_string.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/shared/npruntime/npbridge.h"
#include "native_client/src/shared/npruntime/npcapability.h"
#include "native_client/src/shared/npruntime/npobject_stub.h"

using nacl::NPBridge;
using nacl::NPCapability;
using nacl::NPObjectStub;

// TODO(sehr): Numerous interfaces in this file use char* and length, where
// it would be nicer to use strings.  Do so.

struct SerializedFixed {
  uint32_t type;
  union {
    // BOOLEAN variants use this.
    bool boolean_value;
    // INT32 variants use this.
    int32_t int32_value;
    // STRING variants use this to hold the UTF8Length.
    uint32_t string_length;
  } u;
  // The size of this structure should be 8 bytes on all platforms.
};

struct SerializedDouble {
  struct SerializedFixed fixed;
  double double_value;
  // The size of this structure should be 16 bytes on all platforms.
};

struct SerializedString {
  struct SerializedFixed fixed;
  char string_bytes[8];
  // Any remaining characters immediately follow, and are padded out to
  // the nearest multiple of 8 bytes.
};

struct SerializedObject {
  struct SerializedFixed fixed;
  uint64_t pid;
  uint64_t object;
  // The size of this structure should be 24 bytes on all platforms.
};

namespace {

// Adding value1 and value2 would overflow a nacl_abi_size_t.
bool AddWouldOverflow(size_t value1, size_t value2) {
  return value1 > nacl::kNaClAbiSizeTMax - value2;
}

size_t RoundedNPStringBytes(const NPString& str) {
  // Compute the string length, padded to the nearest multiple of 8.
  if (str.UTF8Length > nacl::kNaClAbiSizeTMax - 8) {
    return nacl::kNaClAbiSizeTMax;
  }
  return (str.UTF8Length + 7) & ~7;
}

nacl_abi_size_t NPVariantSize(const NPVariant* variant) {
  if (NPVARIANT_IS_VOID(*variant) ||
      NPVARIANT_IS_NULL(*variant) ||
      NPVARIANT_IS_BOOLEAN(*variant) ||
      NPVARIANT_IS_INT32(*variant)) {
    return sizeof(SerializedFixed);
  } else if (NPVARIANT_IS_DOUBLE(*variant)) {
    return sizeof(SerializedDouble);
  } else if (NPVARIANT_IS_STRING(*variant)) {
    size_t string_length = RoundedNPStringBytes(NPVARIANT_TO_STRING(*variant));
    if (nacl::kNaClAbiSizeTMax == string_length ||
        string_length > nacl::kNaClAbiSizeTMax - sizeof(SerializedFixed)) {
      // Adding the length to the fixed portion would overflow.
      return 0;
    }
    return
        static_cast<nacl_abi_size_t>(sizeof(SerializedFixed) + string_length);
  } else if (NPVARIANT_IS_OBJECT(*variant)) {
    return sizeof(SerializedObject);
  } else {
    // Unrecognized type.
    return 0;
  }
}

nacl_abi_size_t NPVariantVectorSize(const NPVariant* variants,
                                    uint32_t argc) {
  size_t size = 0;

  for (uint32_t i = 0; i < argc; ++i) {
    size_t element_size = NPVariantSize(&variants[i]);

    if (0 == element_size || AddWouldOverflow(size, element_size)) {
      // Overflow.
      return 0;
    }
    size += element_size;
  }
  return static_cast<nacl_abi_size_t>(size);
}

bool SerializeNPVariant(NPP npp,
                        const NPVariant* variants,
                        uint32_t argc,
                        char* bytes,
                        nacl_abi_size_t length) {
  size_t offset = 0;

  for (uint32_t i = 0; i < argc; ++i) {
    if (offset >= length) {
      // Not enough bytes to put the requested number of NPVariants.
      return false;
    }
    size_t element_size = NPVariantSize(&variants[i]);
    if (0 == element_size || AddWouldOverflow(offset, element_size)) {
      // Overflow.
      return false;
    }

    char* p = bytes + offset;
    SerializedFixed* s = reinterpret_cast<SerializedFixed*>(p);
    s->type = static_cast<uint32_t>(variants[i].type);

    if (NPVARIANT_IS_VOID(variants[i]) || NPVARIANT_IS_NULL(variants[i])) {
      element_size = sizeof(SerializedFixed);
    } else if (NPVARIANT_IS_BOOLEAN(variants[i])) {
      s->u.boolean_value = NPVARIANT_TO_BOOLEAN(variants[i]);
      element_size = sizeof(SerializedFixed);
    } else if (NPVARIANT_IS_INT32(variants[i])) {
      s->u.int32_value = NPVARIANT_TO_BOOLEAN(variants[i]);
      element_size = sizeof(SerializedFixed);
    } else if (NPVARIANT_IS_DOUBLE(variants[i])) {
      SerializedDouble* sd = reinterpret_cast<SerializedDouble*>(p);
      sd->double_value = NPVARIANT_TO_DOUBLE(variants[i]);
      element_size = sizeof(SerializedDouble);
    } else if (NPVARIANT_IS_STRING(variants[i])) {
      NPString str = NPVARIANT_TO_STRING(variants[i]);
      SerializedString* ss = reinterpret_cast<SerializedString*>(p);
      ss->fixed.u.string_length = str.UTF8Length;
      memcpy(reinterpret_cast<void*>(ss->string_bytes),
             reinterpret_cast<void*>(const_cast<NPUTF8*>(str.UTF8Characters)),
             str.UTF8Length);
      element_size = sizeof(SerializedFixed) + RoundedNPStringBytes(str);
    } else if (NPVARIANT_IS_OBJECT(variants[i])) {
      NPObject* object = NPVARIANT_TO_OBJECT(variants[i]);
      // Passing objects is done by passing a capability.
      NPCapability capability;
      NPObjectStub::CreateStub(npp, object, &capability);
      SerializedObject* so = reinterpret_cast<SerializedObject*>(p);
      so->pid= capability.pid();
      so->object = reinterpret_cast<uint64_t>(capability.object());
      element_size = sizeof(SerializedObject);
    } else {
      // Bad type.
      return false;
    }
    offset += element_size;
  }
  return true;
}

bool DeserializeNPVariant(NPP npp,
                          char* bytes,
                          nacl_abi_size_t length,
                          NPVariant* variants,
                          uint32_t argc) {
  char* p = bytes;
  nacl_abi_size_t element_size;

  for (uint32_t i = 0; i < argc; ++i) {
    if (p >= bytes + length) {
      // Not enough bytes to get the requested number of NPVariants.
      return false;
    }
    SerializedFixed* s = reinterpret_cast<SerializedFixed*>(p);

    variants[i].type = static_cast<NPVariantType>(s->type);
    if (NPVARIANT_IS_VOID(variants[i]) || NPVARIANT_IS_NULL(variants[i])) {
      element_size = sizeof(SerializedFixed);
    } else if (NPVARIANT_IS_BOOLEAN(variants[i])) {
      BOOLEAN_TO_NPVARIANT(s->u.boolean_value, variants[i]);
      element_size = sizeof(SerializedFixed);
    } else if (NPVARIANT_IS_INT32(variants[i])) {
      INT32_TO_NPVARIANT(s->u.int32_value, variants[i]);
      element_size = sizeof(SerializedFixed);
    } else if (NPVARIANT_IS_DOUBLE(variants[i])) {
      SerializedDouble* sd = reinterpret_cast<SerializedDouble*>(p);
      DOUBLE_TO_NPVARIANT(sd->double_value, variants[i]);
      element_size = sizeof(SerializedDouble);
    } else if (NPVARIANT_IS_STRING(variants[i])) {
      SerializedString* ss = reinterpret_cast<SerializedString*>(p);
      nacl_abi_size_t string_length = ss->fixed.u.string_length;
      if (AddWouldOverflow(string_length, 7)) {
        // Rounding to the next 8 would overflow.
        return false;
      }
      nacl_abi_size_t rounded_length = (string_length + 7) & ~7;
      if (0 == string_length) {
        STRINGN_TO_NPVARIANT(NULL, 0, variants[i]);
      } else {
        // We need to copy the string payload using memory allocated by
        // NPN_MemAlloc.
        void* copy = NPN_MemAlloc(length + 1);
        if (NULL == copy) {
          // Memory allocation failed.
          return false;
        } else {
          memmove(copy, ss->string_bytes, string_length);
        }
        STRINGN_TO_NPVARIANT(reinterpret_cast<NPUTF8*>(copy),
                             string_length,
                             variants[i]);
      }
      if (AddWouldOverflow(rounded_length, sizeof(SerializedString))) {
        return false;
      }
      element_size = sizeof(SerializedFixed) + rounded_length;
    } else if (NPVARIANT_IS_OBJECT(variants[i])) {
      SerializedObject* so = reinterpret_cast<SerializedObject*>(p);
      NPCapability capability;
      capability.set_pid(so->pid);
      capability.set_object(reinterpret_cast<NPObject*>(so->object));
      NPBridge* bridge = NPBridge::LookupBridge(npp);
      NPObject* object = bridge->CreateProxy(npp, capability);
      if (NULL == object) {
        NULL_TO_NPVARIANT(variants[i]);
      } else {
        OBJECT_TO_NPVARIANT(object, variants[i]);
      }
      element_size = sizeof(SerializedObject);
    } else {
      // Bad type.
      return false;
    }
    p += element_size;
  }
  return true;
}

}  // namespace

namespace nacl {

// TODO(sehr): Add a more general compile time assertion package elsewhere.
#define ASSERT_TYPE_SIZE(struct_name, struct_size) \
    int struct_name##_size_should_be_##struct_size[ \
                    sizeof(struct_name) == struct_size ? 1 : 0]

// Check the wire format sizes for the NPVariant subtypes.
ASSERT_TYPE_SIZE(SerializedFixed, 8);
ASSERT_TYPE_SIZE(SerializedDouble, 16);
ASSERT_TYPE_SIZE(SerializedString, 16);
ASSERT_TYPE_SIZE(SerializedObject, 24);

char* NPVariantsToWireFormat(NPP npp,
                             const NPVariant* variants,
                             uint32_t argc,
                             char* bytes,
                             nacl_abi_size_t* length) {
  // Length needs to be set.
  if (NULL == length) {
    return NULL;
  }
  // No need to do anything if there are no variants to serialize.
  if (0 == argc) {
    *length = 0;
    return NULL;
  }
  // Report an error if no variants are passed but argc > 0.
  if (NULL == variants) {
    return NULL;
  }
  // Compute the size of the buffer.  Zero indicates error.
  nacl_abi_size_t tmp_length = NPVariantVectorSize(variants, argc);
  if (0 == tmp_length || tmp_length > *length) {
    return NULL;
  }
  // Allocate the buffer, if the client didn't pass one.
  bool performed_allocation = false;
  if (NULL == bytes) {
    performed_allocation = true;
    bytes = new(std::nothrow) char[tmp_length];
    if (NULL == bytes) {
      return NULL;
    }
  }
  // Serialize the variants.
  if (!SerializeNPVariant(npp, variants, argc, bytes, tmp_length)) {
    if (performed_allocation) {
      delete variants;
    }
    return NULL;
  }
  // Return success.
  *length = tmp_length;
  return bytes;
}

NPVariant* WireFormatToNPVariants(NPP npp,
                                  char* bytes,
                                  nacl_abi_size_t length,
                                  uint32_t argc,
                                  NPVariant* variants) {
  // No need to allocate memory if there are no variants to be received.
  if (0 == argc) {
    return NULL;
  }
  // Otherwise, there must be some input bytes to get from.
  if (NULL == bytes || 0 == length) {
    return NULL;
  }
  // Allocate memory for the variants, if the client didn't pass one.
  bool performed_allocation = false;
  if (NULL == variants) {
    performed_allocation = true;
    variants = new(std::nothrow) NPVariant[argc];
    if (NULL == variants) {
      return NULL;
    }
  }
  // Read the serialized NPVariants into the allocated memory.
  if (!DeserializeNPVariant(npp, bytes, length, variants, argc)) {
    if (performed_allocation) {
      delete variants;
    }
    return NULL;
  }
  return variants;
}

char* NPCapabilityToWireFormat(NPCapability* capability,
                               char* bytes,
                               nacl_abi_size_t* length) {
  // Check the parameters.
  if (NULL == capability ||
      NULL == length ||
      sizeof(*capability) > *length) {
    return NULL;
  }
  // Allocate the buffer, if the client didn't pass one.
  if (NULL == bytes) {
    bytes = new(std::nothrow) char[sizeof(*capability)];
    if (NULL == bytes) {
      return NULL;
    }
  }
  // Serialization is just a memcpy.
  memcpy(reinterpret_cast<void*>(bytes),
         reinterpret_cast<const void*>(capability),
         static_cast<size_t>(sizeof(*capability)));
  // Return success.
  *length = static_cast<nacl_abi_size_t>(sizeof(*capability));
  return bytes;
}

NPCapability* WireFormatToNPCapability(char* bytes,
                                       nacl_abi_size_t length,
                                       NPCapability* capability) {
  // Check the parameters.
  if (NULL == bytes || sizeof(NPCapability) > length) {
    return NULL;
  }
  // Allocate the capability, if the client didn't pass one.
  if (NULL == capability) {
    capability = new(std::nothrow) NPCapability();
    if (NULL == capability) {
      return NULL;
    }
  }
  // Deserialization is just a memcpy.
  memcpy(reinterpret_cast<void*>(capability),
         reinterpret_cast<const void*>(bytes),
         sizeof(*capability));
  return capability;
}

char* NPObjectToWireFormat(NPP npp,
                           NPObject* object,
                           char* bytes,
                           nacl_abi_size_t* length) {
  // Check the parameters.
  if (NULL == length ||
      sizeof(NPCapability) > *length) {
    return false;
  }
  // Lookup the capability for the stub for the given object.
  NPCapability capability;
  NPObjectStub::CreateStub(npp, object, &capability);
  // Serialize the capability.
  return NPCapabilityToWireFormat(&capability, bytes, length);
}

NPObject* WireFormatToNPObject(NPP npp,
                               char* bytes,
                               nacl_abi_size_t length) {
  // Check the parameters.
  if (NULL == bytes || sizeof(NPCapability) > length) {
    return false;
  }
  // Deserialize a capability.
  NPCapability* capability = WireFormatToNPCapability(bytes, length, NULL);
  if (NULL == capability) {
    return NULL;
  }
  // Lookup the proxy for the capability.
  NPBridge* bridge = NPBridge::LookupBridge(npp);
  NPObject* retval = bridge->CreateProxy(npp, *capability);
  // Clean up.
  delete capability;
  return retval;
}

// NPStrings are serialized as a uint32_t length followed by the bytes.
char* NPStringToWireFormat(NPString* string,
                           char* bytes,
                           nacl_abi_size_t* length) {
  // Check the parameters.
  if (NULL == string ||
      NULL == length ||
      AddWouldOverflow(string->UTF8Length, sizeof(uint32_t)) ||
      *length < string->UTF8Length + sizeof(uint32_t) ||
      (string->UTF8Length > 0 && NULL == string->UTF8Characters)) {
    return NULL;
  }
  size_t tmp_length = sizeof(uint32_t) + string->UTF8Length;
  // Allocate space to hold the serialized string if the client didn't pass one.
  if (NULL == bytes) {
    bytes = new(std::nothrow) char[tmp_length];
    if (NULL == bytes) {
      return NULL;
    }
  }
  // Serialize the string into the buffer.
  *reinterpret_cast<uint32_t*>(bytes) = string->UTF8Length;
  memcpy(bytes + sizeof(uint32_t),
         string->UTF8Characters,
         string->UTF8Length);
  *length = static_cast<nacl_abi_size_t>(tmp_length);
  return bytes;
}

NPString* WireFormatToNPString(char* bytes,
                               nacl_abi_size_t length,
                               NPString* str) {
  // Check the parameters.
  if (NULL == bytes) {
    return NULL;
  }
  // Allocate space for the string bytes and the NPString itself.
  uint32_t str_length = *reinterpret_cast<uint32_t*>(bytes);
  if (length < str_length + sizeof(uint32_t)) {
    return NULL;
  }
  char* str_bytes = reinterpret_cast<char*>(NPN_MemAlloc(str_length));
  if (NULL == str_bytes) {
    return NULL;
  }
  // If the client passed in an address, use it; otherwise allocate new string.
  if (NULL == str) {
    str = new(std::nothrow) NPString;
    if (NULL == str) {
      NPN_MemFree(str_bytes);
      return NULL;
    }
  }
  // Copy the string.
  memcpy(str_bytes,
         bytes + sizeof(uint32_t),
         str_length);
  // Create the NPString.
  str->UTF8Characters = str_bytes;
  str->UTF8Length = str_length;
  return str;
}

}  // namespace nacl
