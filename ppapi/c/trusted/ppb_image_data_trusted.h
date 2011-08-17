/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From trusted/ppb_image_data_trusted.idl modified Sat Jul 16 16:51:03 2011. */

#ifndef PPAPI_C_TRUSTED_PPB_IMAGE_DATA_TRUSTED_H_
#define PPAPI_C_TRUSTED_PPB_IMAGE_DATA_TRUSTED_H_

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_IMAGEDATA_TRUSTED_INTERFACE_0_4 "PPB_ImageDataTrusted;0.4"
#define PPB_IMAGEDATA_TRUSTED_INTERFACE PPB_IMAGEDATA_TRUSTED_INTERFACE_0_4

/**
 * @file
 * This file defines the trusted ImageData interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/** Trusted interface */
struct PPB_ImageDataTrusted {
  /**
   * Returns the internal shared memory pointer associated with the given
   * ImageData resource. Used for proxying. Returns PP_OK on success, or
   * PP_ERROR_* on failure.  On success, the size in bytes of the shared
   * memory region will be placed into |*byte_count|, and the handle for
   * the shared memory in |*handle|.
   */
  int32_t (*GetSharedMemory)(PP_Resource image_data,
                             int* handle,
                             uint32_t* byte_count);
};
/**
 * @}
 */

#endif  /* PPAPI_C_TRUSTED_PPB_IMAGE_DATA_TRUSTED_H_ */

