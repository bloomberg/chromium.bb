/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_resource_array_dev.idl modified Fri Jan  6 11:59:21 2012. */

#ifndef PPAPI_C_DEV_PPB_RESOURCE_ARRAY_DEV_H_
#define PPAPI_C_DEV_PPB_RESOURCE_ARRAY_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_RESOURCEARRAY_DEV_INTERFACE_0_1 "PPB_ResourceArray(Dev);0.1"
#define PPB_RESOURCEARRAY_DEV_INTERFACE PPB_RESOURCEARRAY_DEV_INTERFACE_0_1

/**
 * @file
 * This file defines the <code>PPB_ResourceArray_Dev</code> interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * A resource array holds a list of resources and retains a reference to each of
 * them.
 */
struct PPB_ResourceArray_Dev_0_1 {
  /**
   * Creates a resource array.
   * Note: It will add a reference to each of the elements.
   *
   * @param[in] elements <code>PP_Resource</code>s to be stored in the created
   * resource array.
   * @param[in] size The number of elements.
   *
   * @return A <code>PP_Resource</code> corresponding to a resource array if
   * successful; 0 if failed.
   */
  PP_Resource (*Create)(PP_Instance instance,
                        const PP_Resource elements[],
                        uint32_t size);
  /**
   * Determines if the provided resource is a resource array.
   *
   * @param[in] resource A <code>PP_Resource</code> corresponding to a generic
   * resource.
   *
   * @return A <code>PP_Bool</code> that is <code>PP_TRUE</code> if the given
   * resource is a resource array, otherwise <code>PP_FALSE</code>.
   */
  PP_Bool (*IsResourceArray)(PP_Resource resource);
  /**
   * Gets the array size.
   *
   * @param[in] resource_array The resource array.
   *
   * @return How many elements are there in the array.
   */
  uint32_t (*GetSize)(PP_Resource resource_array);
  /**
   * Gets the element at the specified position.
   * Note: It doesn't add a reference to the returned resource for the caller.
   *
   * @param[in] resource_array The resource array.
   * @param[in] index An integer indicating a position in the array.
   *
   * @return A <code>PP_Resource</code>. Returns 0 if the index is out of range.
   */
  PP_Resource (*GetAt)(PP_Resource resource_array, uint32_t index);
};

typedef struct PPB_ResourceArray_Dev_0_1 PPB_ResourceArray_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_RESOURCE_ARRAY_DEV_H_ */

