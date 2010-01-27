/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#ifndef NATIVE_CLIENT_NPAPI_PLUGIN_SRPC_NPAPI_NATIVE_H_
#define NATIVE_CLIENT_NPAPI_PLUGIN_SRPC_NPAPI_NATIVE_H_

#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/plugin/srpc/plugin.h"
#include "native_client/src/trusted/plugin/srpc/ret_array.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"

#ifndef SIZE_T_MAX
# define SIZE_T_MAX (~((size_t) 0))
#endif  // SIZE_T_MAX

namespace nacl_srpc {

// Extern class declaration.
class Plugin;
template <typename HandleType>
class ScriptableHandle;

// A utility method that gets the length value from an array NPVariant.
// It returns true if the NPVariant is an array, false otherwise.
extern bool NPVariantObjectLength(const NPVariant* variant,
                                  PluginIdentifier npp,
                                  uint32_t* length);

// ScalarToNPVariant converts a given native type to an NPVariant.
// It returns true if the conversion was successful, false otherwise.
template<typename T> bool ScalarToNPVariant(T value, NPVariant* var) {
  // The generic version used for int and uint32_t always succeeds.
  INT32_TO_NPVARIANT(static_cast<int32_t>(value), *var);
  return true;
}

template<> bool ScalarToNPVariant<bool>(bool value, NPVariant* var);
template<> bool ScalarToNPVariant<double>(double value, NPVariant* var);
template<> bool ScalarToNPVariant<char*>(char* value, NPVariant* var);
template<> bool ScalarToNPVariant<const char*>(const char* value,
                                               NPVariant* var);
template<> bool ScalarToNPVariant<NPObject*>(NPObject* value, NPVariant* var);

// NPVariantToScalar extracts a scalar value of the template type from
// an NPVariant value.  If the type of the NPVariant is compatible, it
// sets the value correctly and returns true.  Otherwise it sets the value
// to zero and returns false.
template<typename T> bool NPVariantToScalar(const NPVariant* var,
                                            T* native_value) {
  // Initialize the result for failure cases.
  *native_value = 0;
  if (NPVARIANT_IS_DOUBLE(*var)) {
    // Make the result available to the caller.
    *native_value = static_cast<T>(NPVARIANT_TO_DOUBLE(*var));
    return true;
  } else if (NPVARIANT_IS_INT32(*var)) {
    // Make the result available to the caller.
    *native_value = static_cast<T>(NPVARIANT_TO_INT32(*var));
    return true;
  } else {
    return false;
  }
}

/*
template<> bool NPVariantToScalar<NaClDesc*>(const NPVariant* var,
                                             NaClDesc** value);
                                             */
template<> bool NPVariantToScalar<bool>(const NPVariant* var, bool* b);
template<> bool NPVariantToScalar<char*>(const NPVariant* var, char** s);
template<> bool NPVariantToScalar<NPObject*>(const NPVariant* var,
                                             NPObject** obj);

// NPVariantToArray extracts an array value of the template type from
// an NPVariant value.  If the type of the NPVariant is compatible, it
// sets the value correctly and returns true.  Otherwise it sets the value
// to zero and returns false.
template<typename T> bool NPVariantToArray(
    const NPVariant* nparg,
    PluginIdentifier npp,
    uint32_t* array_length,
    T* array_data) {
  // Initialize result values for error cases.
  *array_length = 0;
  *array_data = NULL;
  // Verify that the NPVariant passed in is an NPObject.
  if (!NPVARIANT_IS_OBJECT(*nparg)) {
    return false;
  }
  NPObject* nparray_object = NPVARIANT_TO_OBJECT(*nparg);
  uint32_t elt_count_uint;
  // Verify that it has a length property and get the length as the element
  // count.
  if (!NPVariantObjectLength(nparg, npp, &elt_count_uint)) {
    return false;
  }
  size_t element_count = static_cast<size_t>(elt_count_uint);
  T tmp_array_data;
  // Check for overflow on size multiplication.
  if (element_count > SIZE_T_MAX / sizeof(*tmp_array_data)) {
    return false;
  }
  tmp_array_data =
      reinterpret_cast<T>(malloc(element_count * sizeof(*tmp_array_data)));
  // Make sure malloc returns a valid pointer.
  if (NULL == tmp_array_data) {
    return false;
  }
  // Limit array size to the IMC maximum message length.  This is necessary
  // but not sufficient, because there could be more than one parameter.
  if (element_count * sizeof(*tmp_array_data) > NACL_ABI_IMC_USER_BYTES_MAX) {
    free(tmp_array_data);
    return false;
  }
  // Get the elements of the object into the allocated array.
  T array_element_pointer = tmp_array_data;
  for (size_t i = 0; i < element_count; ++i) {
    NPIdentifier index_id = NPN_GetIntIdentifier(i);
    NPVariant val;
    // Get the array object property for index i.
    if (!NPN_GetProperty(npp, nparray_object, index_id, &val)) {
      free(tmp_array_data);
      return false;
    }
    // Extract the scalar value from the element.
    bool correct_type = NPVariantToScalar(&val, array_element_pointer);
    // Free the variant for the array element.
    NPN_ReleaseVariantValue(&val);
    if (!correct_type) {
      free(tmp_array_data);
      return false;
    }
    // Move to the next element in the array;
    ++array_element_pointer;
  }
  // Make the element count and data available to the caller.
  *array_length = element_count;
  *array_data = tmp_array_data;
  return true;
}

// NPVariantToAllocatedArray constructs an array value of the template type
// from an NPVariant value specifying the length of the array.  It is used
// for building return value arrays for SRPC method invocations.
template<typename T> bool NPVariantToAllocatedArray(const NPVariant* var,
                                                    uint32_t* array_length,
                                                    T* array_data) {
  // Initialize result values for error cases.
  *array_length = 0;
  *array_data = NULL;
  // The NPVariant for a return array length has to be a value
  // that will be accepted as converting to uint32_t.
  uint32_t elt_count_uint;
  if (!NPVariantToScalar(var, &elt_count_uint)) {
    return false;
  }
  size_t element_count = static_cast<size_t>(elt_count_uint);
  T tmp_array_data;
  // Check for overflow on size multiplication.
  if (element_count > SIZE_T_MAX / sizeof(*tmp_array_data)) {
    return false;
  }
  tmp_array_data =
      reinterpret_cast<T>(malloc(element_count * sizeof(*tmp_array_data)));
  // Make sure malloc returns a valid pointer.
  if (NULL == tmp_array_data) {
    return false;
  }
  // Limit array size to the IMC maximum message length.  This is necessary
  // but not sufficient, because there could be more than one parameter.
  if (element_count * sizeof(*tmp_array_data) > NACL_ABI_IMC_USER_BYTES_MAX) {
    free(tmp_array_data);
    return false;
  }
  // Make the element count and data available to the caller.
  *array_length = element_count;
  *array_data = tmp_array_data;
  return true;
}

}  // namespace nacl_srpc

#endif  // NATIVE_CLIENT_NPAPI_PLUGIN_SRPC_NPAPI_NATIVE_H_
