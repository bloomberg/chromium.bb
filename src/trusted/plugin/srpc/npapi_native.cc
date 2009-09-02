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

#include <stdlib.h>
#include <string.h>

#include "native_client/src/trusted/plugin/srpc/browser_interface.h"
#include "native_client/src/trusted/plugin/srpc/npapi_native.h"
#include "native_client/src/trusted/plugin/srpc/scriptable_handle.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"

namespace nacl_srpc {

// Specializations for ScalarToNPVariant -- to avoid multiply defined issue
// when they are placed in the header.
template<> bool ScalarToNPVariant<bool>(bool value, NPVariant* var) {
  // Boolean specialization always succeeds.
  BOOLEAN_TO_NPVARIANT(value, *var);
  return true;
}

template<> bool ScalarToNPVariant<double>(double value, NPVariant* var) {
  // Double specialization always succeeds.
  DOUBLE_TO_NPVARIANT(value, *var);
  return true;
}

template<> bool ScalarToNPVariant<char*>(char* value, NPVariant* var) {
  return ScalarToNPVariant<const char*>(static_cast<const char*>(value), var);
}

template<> bool ScalarToNPVariant<const char*>(const char* value,
                                               NPVariant* var) {
  // Initialize return value for failure cases.
  VOID_TO_NPVARIANT(*var);
  if (NULL == value) {
    return false;
  }
  size_t length = strlen(value) + 1;
  char *tmpstr = reinterpret_cast<char*>(NPN_MemAlloc(length));
  if (NULL == tmpstr) {
    return false;
  }
  strncpy(tmpstr, value, length);
  // Make result available to the caller.
  STRINGZ_TO_NPVARIANT(tmpstr, *var);
  return true;
}

template<> bool ScalarToNPVariant<NPObject*>(NPObject* value, NPVariant* var) {
  // NPObject specialization always succeeds.
  OBJECT_TO_NPVARIANT(value, *var);
  return true;
}

// Specializations for NPVariantToScalar -- to avoid multiply defined issue
// when they are placed in the header.
template<> bool NPVariantToScalar<NaClDesc*>(const NPVariant* var,
                                             NaClDesc** value) {
  // Initialize return value for failure cases.
  *value = NULL;
  if (!NPVARIANT_IS_OBJECT(*var)) {
    return false;
  }
  NPObject* obj = NPVARIANT_TO_OBJECT(*var);
  ScriptableHandleBase* scriptable_base =
    static_cast<ScriptableHandleBase*>(obj);
  // This function is called only when we are dealing with a DescBasedHandle
  DescBasedHandle *desc_handle =
    static_cast<DescBasedHandle*>(scriptable_base->get_handle());

  // Make result available to the caller.
  *value = desc_handle->desc();
  return true;
}

template<> bool NPVariantToScalar<bool>(const NPVariant* var, bool* b) {
  *b = false;
  if (!NPVARIANT_IS_BOOLEAN(*var)) {
    return false;
  }
  *b = NPVARIANT_TO_BOOLEAN(*var);
  return true;
}

template<> bool NPVariantToScalar<char*>(const NPVariant* var, char** s) {
  // Initialize return value for failure cases.
  *s = NULL;
  if (!NPVARIANT_IS_STRING(*var)) {
    return false;
  }
  size_t length = NPVARIANT_TO_STRING(*var).utf8length;
  // Allocate a copy.
  NPUTF8* result =
    reinterpret_cast<char*>(malloc(length + 1));
  if (NULL == result) {
    return false;
  }
  // Copy to the string .
  memcpy(result, NPVARIANT_TO_STRING(*var).utf8characters, length);
  result[length] = '\0';
  // Make result available to the caller.
  *s = result;
  return true;
}

template<> bool NPVariantToScalar<NPObject*>(const NPVariant* var,
                                             NPObject** obj) {
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
static bool GetNPObjectProperty(nacl_srpc::PluginIdentifier npp,
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
                           nacl_srpc::PluginIdentifier npp,
                           uint32_t* length) {
  // Initialize the length for error returns.
  *length = 0;
  // Get the variant for a property.
  NPVariant nplength;
  if (!GetNPObjectProperty(npp,
                           variant,
                           (NPIdentifier)PortablePluginInterface::kLengthIdent,
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

}  // namespace nacl_srpc
