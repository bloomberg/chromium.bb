/*
 * Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/shared/ppapi_proxy/object_serialize.h"

#include <limits>
#include <stdio.h>
#include <string.h>


#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability_process.h"
#ifdef __native_client__
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#else
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#endif  // __native_client__
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_var.h"

namespace ppapi_proxy {

namespace {

// A serialized string consists of a fixed minimum of 8 bytes.
static const int kStringFixedBytes = 8;
// Followed by a varying number of bytes rounded up to the nearest 8 bytes.
static const uint32_t kStringRoundBase = 8;

}  // namespace

// The basic serialization structure.  Used alone for PP_VARTYPE_VOID,
// PP_VARTYPE_NULL, and PP_VARTYPE_INT32.
struct SerializedFixed {
  uint32_t type;
  union {
    // PP_VARTYPE_BOOLEAN uses this.
    bool boolean_value;
    // PP_VARTYPE_INT32 uses this.
    int32_t int32_value;
    // PP_VARTYPE_STRING uses this.
    uint32_t string_length;
  } u;
  // The size of this structure should be 8 bytes on all platforms.
};

// The structure used for PP_VARTYPE_DOUBLE.
struct SerializedDouble {
  struct SerializedFixed fixed;
  double double_value;
};

// The structure used for PP_VARTYPE_STRING.

struct SerializedString {
  struct SerializedFixed fixed;
  char string_bytes[kStringFixedBytes];
  // Any remaining characters immediately follow, and are padded out to the
  // nearest multiple of kStringRoundBase bytes.
};

// TODO(sehr): Add a more general compile time assertion package elsewhere.
#define ASSERT_TYPE_SIZE(struct_name, struct_size) \
    int struct_name##_size_should_be_##struct_size[ \
                    sizeof(struct_name) == struct_size ? 1 : 0]

// Check the wire format sizes for the PP_Var subtypes.
ASSERT_TYPE_SIZE(SerializedFixed, 8);
ASSERT_TYPE_SIZE(SerializedDouble, 16);
ASSERT_TYPE_SIZE(SerializedString, 16);

//
// We currently use offsetof to find the start of string storage.
// This avoids the (never seen) case where the compiler inserts in
// padding between the struct SerializedFixed fixed header and the
// actual payload value in the double, string, and object
// serialization variants.
//
// Untrusted arm toolchain defines an offsetof in stddef.h, so we have
// to prefix.
//
#define NACL_OFFSETOF(pod_t, member) \
  (static_cast<size_t>(reinterpret_cast<uintptr_t>(&((pod_t *) NULL)->member)))

namespace {

// Adding value1 and value2 would overflow a uint32_t.
bool AddWouldOverflow(size_t value1, size_t value2) {
  if (value1 > std::numeric_limits<size_t>::max() - value2) {
    return true;
  }
  size_t sum = value1 + value2;
  return sum > std::numeric_limits<uint32_t>::max();
}

uint32_t RoundedStringBytes(uint32_t string_length) {
  // Compute the string length, padded to the nearest multiple of 8.
  if (AddWouldOverflow(string_length, kStringRoundBase - 1)) {
    return std::numeric_limits<uint32_t>::max();
  }
  return (string_length + (kStringRoundBase - 1)) & ~(kStringRoundBase - 1);
}

uint32_t PpVarSize(const PP_Var& var) {
  switch (var.type) {
    case PP_VARTYPE_UNDEFINED:
    case PP_VARTYPE_NULL:
    case PP_VARTYPE_BOOL:
    case PP_VARTYPE_INT32:
      return sizeof(SerializedFixed);
    case PP_VARTYPE_DOUBLE:
      return sizeof(SerializedDouble);
    case PP_VARTYPE_STRING: {
      uint32_t string_length;
      (void) PPBVarInterface()->VarToUtf8(var, &string_length);
      string_length = RoundedStringBytes(string_length);
      if (std::numeric_limits<uint32_t>::max() == string_length ||
          AddWouldOverflow(string_length,
                           NACL_OFFSETOF(SerializedString, string_bytes))) {
        // Adding the length to the fixed portion would overflow.
        return 0;
      }
      return static_cast<uint32_t>(NACL_OFFSETOF(SerializedString, string_bytes)
                                   + string_length);
      break;
    }
    case PP_VARTYPE_OBJECT:
    case PP_VARTYPE_ARRAY:
    case PP_VARTYPE_DICTIONARY:
      NACL_NOTREACHED();
      break;
  }
  // Unrecognized type.
  return 0;
}

uint32_t PpVarVectorSize(const PP_Var* vars, uint32_t argc) {
  size_t size = 0;

  for (uint32_t i = 0; i < argc; ++i) {
    size_t element_size = PpVarSize(vars[i]);

    if (0 == element_size || AddWouldOverflow(size, element_size)) {
      // Overflow.
      return 0;
    }
    size += element_size;
  }
  return static_cast<uint32_t>(size);
}

bool SerializePpVar(const PP_Var* vars,
                    uint32_t argc,
                    char* bytes,
                    uint32_t length) {
  size_t offset = 0;

  for (uint32_t i = 0; i < argc; ++i) {
    size_t element_size = PpVarSize(vars[i]);
    if (0 == element_size || AddWouldOverflow(offset, element_size)) {
      // Overflow.
      return false;
    }
    if (offset + element_size > length) {
      // Not enough bytes to put the requested number of PP_Vars.
      return false;
    }

    char* p = bytes + offset;
    SerializedFixed* s = reinterpret_cast<SerializedFixed*>(p);
    s->type = static_cast<uint32_t>(vars[i].type);
    // Set the rest of SerializedFixed to 0, in case the following serialization
    // leaves some of it unchanged.
    s->u.int32_value = 0;

    switch (vars[i].type) {
      case PP_VARTYPE_UNDEFINED:
      case PP_VARTYPE_NULL:
        element_size = sizeof(SerializedFixed);
        break;
      case PP_VARTYPE_BOOL:
        s->u.boolean_value = static_cast<bool>
            (PP_TRUE == vars[i].value.as_bool);
        element_size = sizeof(SerializedFixed);
        break;
      case PP_VARTYPE_INT32:
        s->u.int32_value = vars[i].value.as_int;
        element_size = sizeof(SerializedFixed);
        break;
      case PP_VARTYPE_DOUBLE: {
        SerializedDouble* sd = reinterpret_cast<SerializedDouble*>(p);
        sd->double_value = vars[i].value.as_double;
        element_size = sizeof(SerializedDouble);
        break;
      }
      case PP_VARTYPE_STRING: {
        uint32_t string_length;
        const char* str = PPBVarInterface()->VarToUtf8(vars[i], &string_length);
        SerializedString* ss = reinterpret_cast<SerializedString*>(p);
        ss->fixed.u.string_length = string_length;
        memcpy(reinterpret_cast<void*>(ss->string_bytes),
               reinterpret_cast<const void*>(str),
               string_length);
        // Fill padding bytes with zeros.
        memset(reinterpret_cast<void*>(ss->string_bytes + string_length), 0,
            RoundedStringBytes(string_length) - string_length);
        element_size = NACL_OFFSETOF(SerializedString, string_bytes)
            + RoundedStringBytes(string_length);
        break;
      }
      case PP_VARTYPE_OBJECT:
      case PP_VARTYPE_ARRAY:
      case PP_VARTYPE_DICTIONARY:
        NACL_NOTREACHED();
      default:
        return false;
    }
    offset += element_size;
  }
  return true;
}


//
// Compute how many bytes does the string object to be deserialzed use
// in the serialized format.  On error, return
// std::numeric_limits<uint32_t>::max().  This means we cannot handle
// 2**32-1 byte strings.
//
uint32_t DeserializeStringSize(char* p, uint32_t length) {
  // zero length strings are okay... but not shorter
  if (length < NACL_OFFSETOF(SerializedString, string_bytes)) {
    return std::numeric_limits<uint32_t>::max();
  }
  SerializedString* ss = reinterpret_cast<SerializedString*>(p);
  if (PP_VARTYPE_STRING != ss->fixed.type) {
    return std::numeric_limits<uint32_t>::max();
  }
  uint32_t string_length = ss->fixed.u.string_length;
  string_length = RoundedStringBytes(string_length);
  if (std::numeric_limits<uint32_t>::max() == string_length) {
    return std::numeric_limits<uint32_t>::max();
  }
  if (AddWouldOverflow(NACL_OFFSETOF(SerializedString, string_bytes),
                       string_length)) {
    return std::numeric_limits<uint32_t>::max();
  }
  uint32_t total_bytes = NACL_OFFSETOF(SerializedString, string_bytes)
      + string_length;
  if (total_bytes > length) {
    return std::numeric_limits<uint32_t>::max();
  }
  return total_bytes;
}


//
// Compute the number of bytes that will be consumed by the next
// object, based on its type.  If there aren't enough bytes,
// std::numeric_limits<uint32_t>::max() will be returned.
//
// If element_type_ptr is non-NULL, then the next element's
// (purported) type will be filled in.  Whether this occurs when there
// is an error (e.g., not enough data) is not defined, i.e., only rely
// on it when there's no error.
//
uint32_t DeserializePpVarSize(char* p,
                              uint32_t length,
                              PP_VarType* element_type_ptr) {
  SerializedFixed* sfp;
  if (length < sizeof *sfp) {
    return std::numeric_limits<uint32_t>::max();
  }
  sfp = reinterpret_cast<SerializedFixed*>(p);
  uint32_t expected_element_size = 0;
  //
  // Setting this to zero handles the "default" case.  That can occur
  // because sfp->type can originate from untrusted code, and so the
  // value could actually be outside of the PP_VarType enumeration
  // range.  If we hit one of the cases below, then
  // expected_element_size will be bounded away from zero.
  //
  switch (static_cast<PP_VarType>(sfp->type)) {
    case PP_VARTYPE_UNDEFINED:
    case PP_VARTYPE_NULL:
    case PP_VARTYPE_BOOL:
    case PP_VARTYPE_INT32:
      expected_element_size = sizeof(SerializedFixed);
      break;
    case PP_VARTYPE_DOUBLE:
      expected_element_size = sizeof(SerializedDouble);
      break;
    case PP_VARTYPE_STRING:
      expected_element_size = DeserializeStringSize(p, length);
      if (std::numeric_limits<uint32_t>::max() == expected_element_size) {
        return std::numeric_limits<uint32_t>::max();
      }
      break;
      // NB: No default case to trigger -Wswitch-enum, so changes to
      // PP_VarType w/o corresponding changes here will cause a
      // compile-time error.
    case PP_VARTYPE_OBJECT:
    case PP_VARTYPE_ARRAY:
    case PP_VARTYPE_DICTIONARY:
      NACL_NOTREACHED();
      break;
  }
  if (length < expected_element_size) {
    return std::numeric_limits<uint32_t>::max();
  }
  if (NULL != element_type_ptr) {
    *element_type_ptr = static_cast<PP_VarType>(sfp->type);
  }
  return expected_element_size;
}


//
// This should be invoked only if DeserializePpVarSize succeeds, i.e.,
// there are enough bytes at p.
//
bool DeserializeString(char* p,
                       PP_Var* var,
                       NaClSrpcChannel* channel) {
  SerializedString* ss = reinterpret_cast<SerializedString*>(p);
  uint32_t string_length = ss->fixed.u.string_length;
  // VarFromUtf8 creates a buffer of size string_length using the browser-side
  // memory allocation function, and copies string_length bytes from
  // ss->string_bytes in to that buffer.  The ref count of the returned var is
  // 1.
  *var = PPBVarInterface()->VarFromUtf8(LookupModuleIdForSrpcChannel(channel),
                                        ss->string_bytes,
                                        string_length);
  return true;
}

bool DeserializePpVar(NaClSrpcChannel* channel,
                      char* bytes,
                      uint32_t length,
                      PP_Var* vars,
                      uint32_t argc) {
  char* p = bytes;

  for (uint32_t i = 0; i < argc; ++i) {
    PP_VarType element_type;
    uint32_t element_size = DeserializePpVarSize(p, length, &element_type);
    if (std::numeric_limits<uint32_t>::max() == element_size) {
      return false;
    }
    SerializedFixed* s = reinterpret_cast<SerializedFixed*>(p);

    vars[i].type = element_type;
    switch (element_type) {
      case PP_VARTYPE_UNDEFINED:
      case PP_VARTYPE_NULL:
        break;
      case PP_VARTYPE_BOOL:
        vars[i].value.as_bool = static_cast<PP_Bool>(s->u.boolean_value);
        break;
      case PP_VARTYPE_INT32:
        vars[i].value.as_int = s->u.int32_value;
        break;
      case PP_VARTYPE_DOUBLE: {
        SerializedDouble* sd = reinterpret_cast<SerializedDouble*>(p);
        vars[i].value.as_double = sd->double_value;
        break;
      }
      case PP_VARTYPE_STRING:
        if (!DeserializeString(p, &vars[i], channel)) {
          return false;
        }
        break;
      case PP_VARTYPE_OBJECT:
      case PP_VARTYPE_ARRAY:
      case PP_VARTYPE_DICTIONARY:
        NACL_NOTREACHED();
      default:
        return false;
    }
    p += element_size;
    length -= element_size;
  }
  return true;
}

}  // namespace

bool SerializeTo(const PP_Var* var, char* bytes, uint32_t* length) {
  if (bytes == NULL || length == NULL) {
    return false;
  }
  // Compute the size of the serialized form.  Zero indicates error.
  uint32_t tmp_length = PpVarVectorSize(var, 1);
  if (0 == tmp_length || tmp_length > *length) {
    return false;
  }
  // Serialize the var.
  if (!SerializePpVar(var, 1, bytes, tmp_length)) {
    return false;
  }
  // Return success.
  *length = tmp_length;
  return true;
}

char* Serialize(const PP_Var* vars, uint32_t argc, uint32_t* length) {
  // Length needs to be set.
  if (NULL == length) {
    return NULL;
  }
  // No need to do anything if there are no vars to serialize.
  if (0 == argc) {
    *length = 0;
    return NULL;
  }
  // Report an error if no vars are passed but argc > 0.
  if (NULL == vars) {
    return NULL;
  }
  // Compute the size of the buffer.  Zero indicates error.
  uint32_t tmp_length = PpVarVectorSize(vars, argc);
  if (0 == tmp_length || tmp_length > *length) {
    return NULL;
  }
  // Allocate the buffer, if the client didn't pass one.
  char* bytes = new char[tmp_length];
  if (NULL == bytes) {
    return NULL;
  }
  // Serialize the vars.
  if (!SerializePpVar(vars, argc, bytes, tmp_length)) {
    delete[] bytes;
    return NULL;
  }
  // Return success.
  *length = tmp_length;
  return bytes;
}

bool DeserializeTo(NaClSrpcChannel* channel,
                   char* bytes,
                   uint32_t length,
                   uint32_t argc,
                   PP_Var* vars) {
  // Deserializing a zero-length vector is trivially done.
  if (0 == argc) {
    return true;
  }
  // Otherwise, there must be some input bytes to get from.
  if (NULL == bytes || 0 == length) {
    return false;
  }
  // And there has to be a valid address to deserialize to.
  if (NULL == vars) {
    return false;
  }
  // Read the serialized PP_Vars into the allocated memory.
  if (!DeserializePpVar(channel, bytes, length, vars, argc)) {
    return false;
  }
  return true;
}

}  // namespace ppapi_proxy
