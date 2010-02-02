// Copyright (c) 2008 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <limits>

#include "native_client/src/shared/npruntime/nprpc.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/include/portability_string.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/shared/npruntime/npbridge.h"
#include "native_client/src/shared/npruntime/npobject_stub.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

//
// STRINGZ_TO_NPVARIANT considered harmful
//
// The STRINGZ_TO_NPVARIANT macro assigns the return value from strlen()
// to a uint32_t. On 64 bit systems this represents a possible integer
// overflow. Do not use this macro.
//
// TODO(sehr): make this fix in npruntime.h (requires changes to Chrome tree)
//
#undef STRINGZ_TO_NPVARIANT

//
// Instead, use this.
//
const size_t kNPRuntimeStringMax = std::numeric_limits<uint32_t>::max() - 1;
static uint32_t NPStringLenSaturate(const char* str) {
  size_t len = strlen(str);
  if (len > kNPRuntimeStringMax) {
    len = kNPRuntimeStringMax;
  }
  return static_cast<uint32>(len);
}
#define STRINGZ_TO_NPVARIANT(_val, _v) NP_BEGIN_MACRO \
  (_v).type = NPVariantType_String;                   \
  NPString str = {                                    \
    _val,                                             \
    NPStringLenSaturate(_val)                         \
  };                                                  \
  (_v).value.stringValue = str;                       \
  NP_END_MACRO

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
    if (0 == NPVARIANT_TO_STRING(*variant).UTF8Length &&
        NULL == NPVARIANT_TO_STRING(*variant).UTF8Characters) {
      // Odd, we were passed a NULL string (length == 0 or characters == NULL).
      STRINGN_TO_NPVARIANT(NULL, 0, *variant);
    } else {
      uint32_t length = NPVARIANT_TO_STRING(*variant).UTF8Length;
      char* string = reinterpret_cast<char*>(optional_.Request(length));
      if (NULL == string) {
        // Not enough bytes to read string.
        return &null;
      }
      STRINGZ_TO_NPVARIANT(string, *variant);
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
  if (NPVARIANT_IS_STRING(*variant)) {
    if (0 == NPVARIANT_TO_STRING(*variant).UTF8Length ||
        NULL == NPVARIANT_TO_STRING(*variant).UTF8Characters) {
      // Something's wrong with this string.
      printf("malformed string\n");
      return false;
    } else {
      size_t len = NPVARIANT_TO_STRING(*variant).UTF8Length;
      const char* npstr = NPVARIANT_TO_STRING(*variant).UTF8Characters;
      void* str = optional_.Request(len);
      if (NULL == str) {
        // There aren't enough bytes to store the string.
        printf("not enough bytes for string %d\n", static_cast<int>(len));
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
    NPObjectStub::CreateStub(npp_, object, capability);
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
  NPCapability* capability =
      reinterpret_cast<NPCapability*>(fixed_.Request(sizeof(NPCapability)));
  if (NULL == capability) {
    return false;
  }
  fixed_.Consume(sizeof(NPCapability));
  NPObjectStub::CreateStub(npp_, object, capability);
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

//  Work in progress -- better serialization/deserialization of NPVariant.
//  Needed for 64-bit.
class NPVariantWrapper : public NPVariant {
  struct Serialized {
    uint32_t type_;
    union {
      bool bval_;
      uint32_t uval_;
      int32_t ival_;
    } u1_;
    union {
      uint64_t pval_;
      double dval_;
      char sval_[1];
    } u2_;
  };

 public:
  NPVariantWrapper(nacl_abi_size_t length, char* bytes) :
    length_(length), bytes_(bytes) {
      // Note: because of sharing between the RPC system and the wrapper,
      // the wrapper is never given sole ownership of the bytes_ array.
  }

  ~NPVariantWrapper() {
  }
  static NPVariantWrapper* MakeSerialized(NPVariant* variant, uint32_t argc) {
    nacl_abi_size_t length = GetSize(variant, argc);
    if (0 == length) {
      return NULL;
    }
    char* bytes = new(std::nothrow) char[length];
    if (NULL == bytes) {
      return NULL;
    }
    NPVariantWrapper* wrapper =
        new(std::nothrow) NPVariantWrapper(length, bytes);
    if (NULL == wrapper) {
      return NULL;
    }
    if (!wrapper->Serialize(variant, argc)) {
      delete wrapper;
      delete bytes;
      return NULL;
    }
    return wrapper;
  }

  NPVariant* Deserialize(uint32_t argc) {
    NPVariant* variant = new(std::nothrow) NPVariant[argc];
    if (NULL == variant) {
      return NULL;
    }
    if (!Deserialize(argc, variant)) {
      delete variant;
    }
    return variant;
  }

 private:
  static const size_t kNaClAbiSizeTMax =
      static_cast<size_t>(~static_cast<nacl_abi_size_t>(0));

  static size_t StringLength(const NPString& str) {
    // NB: str.UTF8Length < kNaClAbiSizeTMax, because both are the same
    // precision as uint32_t.
    if (str.UTF8Length < sizeof(uint64_t)) {
      return sizeof(uint64_t);
    } else {
      return str.UTF8Length - sizeof(uint64_t);
    }
  }

  static nacl_abi_size_t GetSize(NPVariant* variant, uint32_t argc) {
    size_t size = 0;

    for (uint32_t i = 0; i < argc; ++i) {
      if (size < kNaClAbiSizeTMax - sizeof(Serialized)) {
        // Overflow.
        return 0;
      }
      size += sizeof(Serialized);
      if (NPVARIANT_IS_STRING(*variant)) {
        NPString str = NPVARIANT_TO_STRING(*variant);
        size_t len = StringLength(str);
        if (size < kNaClAbiSizeTMax - len) {
          // Overflow.
          return 0;
        }
        size += len;
      }
    }
    return static_cast<nacl_abi_size_t>(size);
  }

  bool Serialize(NPVariant* variant, uint32_t argc) {
    char* p = bytes_;

    for (uint32_t i = 0; i < argc; ++i) {
      Serialized* s = reinterpret_cast<Serialized*>(p);

      s->type_ = static_cast<uint32_t>(variant[i].type);
      if (NPVARIANT_IS_STRING(variant[i])) {
        // Strings require the fixed portion plus an optional variable-length
        // portion.
        NPString str = NPVARIANT_TO_STRING(variant[i]);
        s->u1_.uval_ = str.UTF8Length;
        memcpy(reinterpret_cast<void*>(s->u2_.sval_),
               reinterpret_cast<void*>(const_cast<NPUTF8*>(str.UTF8Characters)),
               str.UTF8Length);
        p += sizeof(Serialized) - sizeof(uint64_t) + str.UTF8Length;
      } else {
        // All other sub-types use the same fixed portion, at some slight
        // overhead for the smaller types.
        if (NPVARIANT_IS_VOID(variant[i]) || NPVARIANT_IS_NULL(variant[i])) {
        } else if (NPVARIANT_IS_BOOLEAN(variant[i])) {
          s->u1_.bval_ = NPVARIANT_TO_BOOLEAN(variant[i]);
        } else if (NPVARIANT_IS_INT32(variant[i])) {
          s->u1_.ival_ = NPVARIANT_TO_BOOLEAN(variant[i]);
        } else if (NPVARIANT_IS_DOUBLE(variant[i])) {
          s->u2_.dval_ = NPVARIANT_TO_DOUBLE(variant[i]);
        } else if (NPVARIANT_IS_OBJECT(variant[i])) {
          // TODO(sehr): what do we do here?
        } else {
          // Bad type.
          return false;
        }
        p += sizeof(Serialized);
      }
    }
    return true;
  }

  bool Deserialize(uint32_t argc, NPVariant* variant) {
    char* p = bytes_;

    for (uint32_t i = 0; i < argc; ++i) {
      if (p >= bytes_ + length_) {
        // Not enough bytes to get the requested number of NPVariants.
        return false;
      }
      Serialized* s = reinterpret_cast<Serialized*>(p);

      variant[i].type = static_cast<NPVariantType>(s->type_);
      if (NPVARIANT_IS_STRING(variant[i])) {
        // Users of NPVariant always have to copy the string bytes if they
        // wish to retain them, so it's ok to keep pointing into bytes_.
        STRINGN_TO_NPVARIANT(
            static_cast<const NPUTF8*>(s->u2_.sval_),
            s->u1_.uval_,
            variant[i]);
        p += sizeof(Serialized) - sizeof(uint64_t) + s->u1_.uval_;
      } else {
        if (NPVARIANT_IS_VOID(variant[i]) || NPVARIANT_IS_NULL(variant[i])) {
        } else if (NPVARIANT_IS_BOOLEAN(variant[i])) {
          BOOLEAN_TO_NPVARIANT(s->u1_.bval_, variant[i]);
        } else if (NPVARIANT_IS_INT32(variant[i])) {
          INT32_TO_NPVARIANT(s->u1_.ival_, variant[i]);
        } else if (NPVARIANT_IS_DOUBLE(variant[i])) {
          DOUBLE_TO_NPVARIANT(s->u2_.dval_, variant[i]);
        } else if (NPVARIANT_IS_OBJECT(variant[i])) {
          // TODO(sehr): what do we do here?
        } else {
          // Bad type.
          return false;
        }
        p += sizeof(Serialized);
      }
    }
    return true;
  }

 private:
  NPVariantWrapper(const NPVariantWrapper&);
  void operator=(const NPVariantWrapper&);

  int32_t length_;
  char* bytes_;
};

}  // namespace nacl
