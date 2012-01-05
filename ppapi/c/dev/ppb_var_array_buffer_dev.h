/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_var_array_buffer_dev.idl modified Wed Dec 14 18:08:00 2011. */

#ifndef PPAPI_C_DEV_PPB_VAR_ARRAY_BUFFER_DEV_H_
#define PPAPI_C_DEV_PPB_VAR_ARRAY_BUFFER_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_VAR_ARRAY_BUFFER_DEV_INTERFACE_0_1 "PPB_VarArrayBuffer(Dev);0.1"
#define PPB_VAR_ARRAY_BUFFER_DEV_INTERFACE \
    PPB_VAR_ARRAY_BUFFER_DEV_INTERFACE_0_1

/**
 * @file
 * This file defines the <code>PPB_VarArrayBuffer_Dev</code> struct.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * PPB_VarArrayBuffer_Dev API. This provides a way to interact with JavaScript
 * ArrayBuffers, which represent a contiguous sequence of bytes. To manage the
 * reference count for a VarArrayBuffer, please see PPB_Var. Note that
 * these Vars are not part of the embedding page's DOM, and can only be shared
 * with JavaScript via pp::Instance's PostMessage and HandleMessage functions.
 */
struct PPB_VarArrayBuffer_Dev_0_1 {
  /**
   * Create a zero-initialized VarArrayBuffer.
   *
   * @param[in] size_in_bytes The size of the array buffer that will be created.
   *
   * @return A PP_Var which represents an VarArrayBuffer of the requested size
   *         with a reference count of 1.
   */
  struct PP_Var (*Create)(uint32_t size_in_bytes);
  /**
   * Returns the length of the VarArrayBuffer in bytes.
   *
   * @return The length of the VarArrayBuffer in bytes.
   */
  uint32_t (*ByteLength)(struct PP_Var array);
  /**
   * Returns a pointer to the beginning of the buffer for the given array.
   *
   * @param[in] array The array whose buffer should be returned.
   *
   * @return A pointer to the buffer for this array.
   */
  void* (*Map)(struct PP_Var array);
};

typedef struct PPB_VarArrayBuffer_Dev_0_1 PPB_VarArrayBuffer_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_VAR_ARRAY_BUFFER_DEV_H_ */

