/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From trusted/ppb_buffer_trusted.idl modified Wed Oct  5 14:06:02 2011. */

#ifndef PPAPI_C_TRUSTED_PPB_BUFFER_TRUSTED_H_
#define PPAPI_C_TRUSTED_PPB_BUFFER_TRUSTED_H_

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_BUFFER_TRUSTED_INTERFACE_0_1 "PPB_BufferTrusted;0.1"
#define PPB_BUFFER_TRUSTED_INTERFACE PPB_BUFFER_TRUSTED_INTERFACE_0_1

/**
 * @file
 * This file defines the trusted buffer interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_BufferTrusted_0_1 {
  /**
   * Returns the internal shared memory pointer associated with the given
   * Buffer resource.  Used for proxying.  Returns PP_OK on success, or
   * PP_ERROR_* on failure.  On success, the size in bytes of the shared
   * memory region will be placed into |*byte_count|, and the handle for
   * the shared memory in |*handle|.
   */
  int32_t (*GetSharedMemory)(PP_Resource buffer, int* handle);
};

typedef struct PPB_BufferTrusted_0_1 PPB_BufferTrusted;
/**
 * @}
 */

#endif  /* PPAPI_C_TRUSTED_PPB_BUFFER_TRUSTED_H_ */

