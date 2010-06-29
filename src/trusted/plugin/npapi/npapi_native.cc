/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <stdlib.h>
#include <string.h>

#include "native_client/src/include/checked_cast.h"
#include "native_client/src/trusted/plugin/npapi/npapi_native.h"
#include "native_client/src/trusted/plugin/npapi/scriptable_impl_npapi.h"
#include "native_client/src/trusted/plugin/srpc/browser_interface.h"
#include "native_client/src/trusted/plugin/srpc/desc_based_handle.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"

using nacl::assert_cast;

#define STRINGZ_TO_NPVARIANT_ASSERT(_val, _v)                   \
  NP_BEGIN_MACRO                                                \
  (_v).type = NPVariantType_String;                             \
  NPString str = { _val, assert_cast<uint32_t>(strlen(_val)) }; \
  (_v).value.stringValue = str;                                 \
  NP_END_MACRO

namespace plugin {

// Overloads for ScalarToNPVariant -- to avoid multiply defined issue
// when they are placed in the header.
bool ScalarToNPVariant(bool value, NPVariant* var) {
  // Boolean specialization always succeeds.
  BOOLEAN_TO_NPVARIANT(value, *var);
  return true;
}

bool ScalarToNPVariant(double value, NPVariant* var) {
  // Double specialization always succeeds.
  DOUBLE_TO_NPVARIANT(value, *var);
  return true;
}

bool ScalarToNPVariant(char* value, NPVariant* var) {
  return ScalarToNPVariant(static_cast<const char*>(value), var);
}

bool ScalarToNPVariant(const char* value, NPVariant* var) {
  // Initialize return value for failure cases.
  VOID_TO_NPVARIANT(*var);
  if (NULL == value) {
    return false;
  }
  uint32_t length = assert_cast<uint32_t>(strlen(value) + 1);
  char* tmpstr = reinterpret_cast<char*>(NPN_MemAlloc(length));
  if (NULL == tmpstr) {
    return false;
  }
  strncpy(tmpstr, value, length);
  // Make result available to the caller.
  STRINGZ_TO_NPVARIANT_ASSERT(tmpstr, *var);
  return true;
}

bool ScalarToNPVariant(NPObject* value, NPVariant* var) {
  // NPObject specialization always succeeds.
  OBJECT_TO_NPVARIANT(value, *var);
  return true;
}

// Overloads for NPVariantToScalar -- to avoid multiply defined issue
// when they are placed in the header.
bool NPVariantToScalar(const NPVariant* var, NaClDesc** value) {
  // Initialize return value for failure cases.
  *value = NULL;
  if (!NPVARIANT_IS_OBJECT(*var)) {
    return false;
  }
  NPObject* obj = NPVARIANT_TO_OBJECT(*var);
  ScriptableImplNpapi* scriptable_handle =
      static_cast<ScriptableImplNpapi*>(obj);
  // This function is called only when we are dealing with a DescBasedHandle
  PortableHandle* desc_handle = scriptable_handle->handle();
  // Make result available to the caller.
  *value = desc_handle->desc();
  return (NULL != *value);
}

bool NPVariantToScalar(const NPVariant* var, bool* b) {
  *b = false;
  if (!NPVARIANT_IS_BOOLEAN(*var)) {
    return false;
  }
  *b = NPVARIANT_TO_BOOLEAN(*var);
  return true;
}

bool NPVariantToScalar(const NPVariant* var, char** s) {
  // Initialize return value for failure cases.
  *s = NULL;
  if (!NPVARIANT_IS_STRING(*var)) {
    return false;
  }
  size_t length = NPVARIANT_TO_STRING(*var).UTF8Length;
  // Allocate a copy.
  NPUTF8* result =
    reinterpret_cast<char*>(malloc(length + 1));
  if (NULL == result) {
    return false;
  }
  // Copy to the string .
  memcpy(result, NPVARIANT_TO_STRING(*var).UTF8Characters, length);
  result[length] = '\0';
  // Make result available to the caller.
  *s = result;
  return true;
}

bool NPVariantToScalar(const NPVariant* var, NPObject** obj) {
  // Initialize return value for failure cases.
  *obj = NULL;
  // Check that the variant is in fact an object.
  if (!NPVARIANT_IS_OBJECT(*var)) {
    return false;
  }
  // Make result available to the caller.
  *obj = NPVARIANT_TO_OBJECT(*var);
  return true;
}

// Utility function to get the property of an object.
static bool GetNPObjectProperty(InstanceIdentifier npp,
                                const NPVariant* variant,
                                NPIdentifier ident,
                                NPVariant* value) {
  // Initialize return value for failure cases.
  VOID_TO_NPVARIANT(*value);
  // Check that the variant is in fact an object.
  NPObject* nparray_object;
  if (!NPVariantToScalar(variant, &nparray_object)) {
    return false;
  }
  // Get the property, or return failure if there is none.
  NPVariant tmp_value;
  if (!NPN_GetProperty(npp, nparray_object, ident, &tmp_value)) {
    return false;
  }
  // Make result available to the caller.
  *value = tmp_value;
  return true;
}

// Utility function to get the length property of an array object.
bool NPVariantObjectLength(const NPVariant* variant,
                           InstanceIdentifier npp,
                           uint32_t* length) {
  // Initialize the length for error returns.
  *length = 0;
  // Get the variant for a property.
  NPVariant nplength;
  if (!GetNPObjectProperty(npp,
                           variant,
                           PluginNpapi::kLengthIdent,
                           &nplength)) {
    return false;
  }
  // Ensure that the length property was set to an integer value.
  if (!NPVARIANT_IS_INT32(nplength)) {
    NPN_ReleaseVariantValue(&nplength);
    return false;
  }
  // Get the integer value from the variant.
  uint32_t tmp_length;
  bool correct_type = NPVariantToScalar(&nplength, &tmp_length);
  // Release the length NPVariant value.
  NPN_ReleaseVariantValue(&nplength);
  if (!correct_type) {
    return false;
  }
  // Make result available to the caller.
  *length = tmp_length;
  return true;
}

}  // namespace plugin
