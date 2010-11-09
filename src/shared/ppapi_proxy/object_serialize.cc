/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/shared/ppapi_proxy/object_serialize.h"

#include <string.h>
#include <limits>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability_process.h"
#ifdef __native_client__
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#else
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#endif  // __native_client__
#include "native_client/src/shared/ppapi_proxy/object.h"
#include "native_client/src/shared/ppapi_proxy/object_capability.h"
#include "native_client/src/shared/ppapi_proxy/object_proxy.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
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

// The structure used for PP_VARTYPE_OBJECT.
struct SerializedObject {
  struct SerializedFixed fixed;
  ObjectCapability capability;
};

// TODO(sehr): Add a more general compile time assertion package elsewhere.
#define ASSERT_TYPE_SIZE(struct_name, struct_size) \
    int struct_name##_size_should_be_##struct_size[ \
                    sizeof(struct_name) == struct_size ? 1 : 0]

// Check the wire format sizes for the PP_Var subtypes.
ASSERT_TYPE_SIZE(SerializedFixed, 8);
ASSERT_TYPE_SIZE(SerializedDouble, 16);
ASSERT_TYPE_SIZE(SerializedString, 16);
ASSERT_TYPE_SIZE(SerializedObject, 24);

namespace {

// Adding value1 and value2 would overflow a uint32_t.
bool AddWouldOverflow(size_t value1, size_t value2) {
  return value1 > std::numeric_limits<uint32_t>::max() - value2;
}

uint32_t RoundedStringBytes(uint32_t string_length) {
  // Compute the string length, padded to the nearest multiple of 8.
  if (string_length > std::numeric_limits<uint32_t>::max() - kStringRoundBase) {
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
      (void) VarInterface()->VarToUtf8(var, &string_length);
      string_length = RoundedStringBytes(string_length);
      if (std::numeric_limits<uint32_t>::max() == string_length ||
          AddWouldOverflow(string_length, sizeof(SerializedFixed))) {
        // Adding the length to the fixed portion would overflow.
        return 0;
      }
      return static_cast<uint32_t>(sizeof(SerializedFixed) + string_length);
      break;
    }
    case PP_VARTYPE_OBJECT:
      return sizeof(SerializedObject);
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

}  // namespace

bool SerializePpVar(const PP_Var* vars,
                    uint32_t argc,
                    char* bytes,
                    uint32_t length) {
  size_t offset = 0;

  for (uint32_t i = 0; i < argc; ++i) {
    if (offset >= length) {
      // Not enough bytes to put the requested number of PP_Vars.
      return false;
    }
    size_t element_size = PpVarSize(vars[i]);
    if (0 == element_size || AddWouldOverflow(offset, element_size)) {
      // Overflow.
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
        const char* str = VarInterface()->VarToUtf8(vars[i], &string_length);
        SerializedString* ss = reinterpret_cast<SerializedString*>(p);
        ss->fixed.u.string_length = string_length;
        memcpy(reinterpret_cast<void*>(ss->string_bytes),
               reinterpret_cast<const void*>(str),
               string_length);
        // Fill padding bytes with zeros.
        memset(reinterpret_cast<void*>(ss->string_bytes + string_length), 0,
            RoundedStringBytes(string_length) - string_length);
        element_size =
              sizeof(SerializedFixed) + RoundedStringBytes(string_length);
        break;
      }
      case PP_VARTYPE_OBJECT: {
        // Passing objects is done by passing a capability.
        ObjectCapability capability(GETPID(), vars[i].value.as_id);
        // TODO(sehr): create/lookup a stub here.
        // NPObjectStub::CreateStub(npp, object, &capability);
        SerializedObject* so = reinterpret_cast<SerializedObject*>(p);
        so->capability = capability;
        element_size = sizeof(SerializedObject);
        break;
      }
      default:
        return false;
    }
    offset += element_size;
  }
  return true;
}

bool DeserializeString(char* p,
                       PP_Var* var,
                       uint32_t* element_size,
                       NaClSrpcChannel* channel) {
  SerializedString* ss = reinterpret_cast<SerializedString*>(p);
  uint32_t string_length = ss->fixed.u.string_length;
  if (AddWouldOverflow(string_length, kStringRoundBase - 1)) {
    // Rounding to the next 8 would overflow.
    return false;
  }
  uint32_t rounded_length = RoundedStringBytes(string_length);
  if (0 == string_length) {
    // Zero-length string.  Rely on what the PPB_Var_Deprecated does.
    *var = VarInterface()->VarFromUtf8(LookupModuleIdForSrpcChannel(channel),
                                       ss->string_bytes,
                                       0);
  } else {
    // We need to copy the string payload using memory allocated by
    // NPN_MemAlloc.
    void* copy = CoreInterface()->MemAlloc(string_length + 1);
    if (NULL == copy) {
      // Memory allocation failed.
      return false;
    } else {
      memmove(copy, ss->string_bytes, string_length);
    }
    *var = VarInterface()->VarFromUtf8(LookupModuleIdForSrpcChannel(channel),
                                       reinterpret_cast<const char*>(copy),
                                       string_length);
  }
  // Compute the "element_size", or offset in the serialized form from
  // the serialized string that we just read.
  if (AddWouldOverflow(rounded_length, sizeof(SerializedFixed))) {
    return false;
  }
  *element_size = sizeof(SerializedFixed) + rounded_length;
  return true;
}

bool DeserializePpVar(NaClSrpcChannel* channel,
                      char* bytes,
                      uint32_t length,
                      PP_Var* vars,
                      uint32_t argc) {
  char* p = bytes;
  uint32_t element_size;

  for (uint32_t i = 0; i < argc; ++i) {
    if (p >= bytes + length) {
      // Not enough bytes to get the requested number of PP_Vars.
      return false;
    }
    SerializedFixed* s = reinterpret_cast<SerializedFixed*>(p);

    vars[i].type = static_cast<PP_VarType>(s->type);
    switch (vars[i].type) {
      case PP_VARTYPE_UNDEFINED:
      case PP_VARTYPE_NULL:
        element_size = sizeof(SerializedFixed);
        break;
      case PP_VARTYPE_BOOL:
        vars[i].value.as_bool = static_cast<PP_Bool>(s->u.boolean_value);
        element_size = sizeof(SerializedFixed);
        break;
      case PP_VARTYPE_INT32:
        vars[i].value.as_int = s->u.int32_value;
        element_size = sizeof(SerializedFixed);
        break;
      case PP_VARTYPE_DOUBLE: {
        SerializedDouble* sd = reinterpret_cast<SerializedDouble*>(p);
        vars[i].value.as_double = sd->double_value;
        element_size = sizeof(SerializedDouble);
        break;
      }
      case PP_VARTYPE_STRING:
        if (!DeserializeString(p, &vars[i], &element_size, channel)) {
          return false;
        }
        break;
      case PP_VARTYPE_OBJECT: {
        SerializedObject* so = reinterpret_cast<SerializedObject*>(p);
        ObjectCapability capability = so->capability;
        vars[i] = ObjectProxy::New(capability, channel);
        element_size = sizeof(SerializedObject);
        break;
      }
      default:
        return false;
    }
    p += element_size;
  }
  return true;
}

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
  char* bytes = new(std::nothrow) char[tmp_length];
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
