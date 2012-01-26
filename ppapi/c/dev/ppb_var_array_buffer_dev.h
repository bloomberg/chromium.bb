/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_var_array_buffer_dev.idl modified Thu Jan 26 11:25:54 2012. */

#ifndef PPAPI_C_DEV_PPB_VAR_ARRAY_BUFFER_DEV_H_
#define PPAPI_C_DEV_PPB_VAR_ARRAY_BUFFER_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_VAR_ARRAY_BUFFER_DEV_INTERFACE_0_2 "PPB_VarArrayBuffer(Dev);0.2"
#define PPB_VAR_ARRAY_BUFFER_DEV_INTERFACE \
    PPB_VAR_ARRAY_BUFFER_DEV_INTERFACE_0_2

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
struct PPB_VarArrayBuffer_Dev_0_2 {
  /**
   * Create a zero-initialized VarArrayBuffer.
   *
   * @param[in] size_in_bytes The size of the ArrayBuffer that will be created.
   *
   * @return A PP_Var which represents a VarArrayBuffer of the requested size
   *         with a reference count of 1.
   */
  struct PP_Var (*Create)(uint32_t size_in_bytes);
  /**
   * Retrieves the length of the VarArrayBuffer in bytes. On success,
   * byte_length is set to the length of the given ArrayBuffer var. On failure,
   * byte_length is unchanged (this could happen, for instance, if the given
   * PP_Var is not of type PP_VARTYPE_ARRAY_BUFFER). Note that ByteLength() will
   * successfully retrieve the the size of an ArrayBuffer even if the
   * ArrayBuffer is not currently mapped.
   *
   * @param[in] array The ArrayBuffer whose length should be returned.
   *
   * @param[out] byte_length A variable which is set to the length of the given
   *             ArrayBuffer on success.
   *
   * @return PP_TRUE on success, PP_FALSE on failure.
   */
  PP_Bool (*ByteLength)(struct PP_Var array, uint32_t* byte_length);
  /**
   * Maps the ArrayBuffer in to the module's address space and returns a pointer
   * to the beginning of the buffer for the given ArrayBuffer PP_Var. Note that
   * calling Map() can be a relatively expensive operation. Use care when
   * calling it in performance-critical code. For example, you should call it
   * only once when looping over an ArrayBuffer:
   *
   * <code>
   *   char* data = (char*)(array_buffer_if.Map(array_buffer_var));
   *   uint32_t byte_length = 0;
   *   PP_Bool ok = array_buffer_if.ByteLength(array_buffer_var, &byte_length);
   *   if (!ok)
   *     return DoSomethingBecauseMyVarIsNotAnArrayBuffer();
   *   for (uint32_t i = 0; i < byte_length; ++i)
   *     data[i] = 'A';
   * </code>
   *
   * @param[in] array The ArrayBuffer whose internal buffer should be returned.
   *
   * @return A pointer to the internal buffer for this ArrayBuffer. Returns NULL
   *         if the given PP_Var is not of type PP_VARTYPE_ARRAY_BUFFER.
   */
  void* (*Map)(struct PP_Var array);
  /**
   * Unmaps the given ArrayBuffer var from the module address space. Use this if
   * you want to save memory but might want to Map the buffer again later. The
   * PP_Var remains valid and should still be released using PPB_Var when you
   * are done with the ArrayBuffer.
   *
   * @param[in] array The ArrayBuffer which should be released.
   */
  void (*Unmap)(struct PP_Var array);
};

typedef struct PPB_VarArrayBuffer_Dev_0_2 PPB_VarArrayBuffer_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_VAR_ARRAY_BUFFER_DEV_H_ */

