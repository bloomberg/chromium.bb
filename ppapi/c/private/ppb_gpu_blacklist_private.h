/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/ppb_gpu_blacklist_private.idl,
 *   modified Wed Oct 24 14:41:20 2012.
 */

#ifndef PPAPI_C_PRIVATE_PPB_GPU_BLACKLIST_PRIVATE_H_
#define PPAPI_C_PRIVATE_PPB_GPU_BLACKLIST_PRIVATE_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_macros.h"

#define PPB_GPUBLACKLIST_PRIVATE_INTERFACE_0_2 "PPB_GpuBlacklist_Private;0.2"
#define PPB_GPUBLACKLIST_PRIVATE_INTERFACE \
    PPB_GPUBLACKLIST_PRIVATE_INTERFACE_0_2

/**
 * @file
 * This file contains the <code>PPB_FileRefPrivate</code> interface. */


/**
 * @addtogroup Interfaces
 * @{
 */
/** PPB_GpuBlacklist_Private interface */
struct PPB_GpuBlacklist_Private_0_2 {
  /**
   * Returns true if the current system's GPU is blacklisted and 3D in Chrome
   * will be emulated via software rendering.
   *
   * This is used internally by the SRPC NaCl proxy (not exposed to plugins) to
   * determine if the 3D interfaces should be exposed to plugins. We don't
   * expose the 3D interfaces if the 3D support is software-emulated. When the
   * SRPC proxy is removed, this interface can also be removed.
   */
  PP_Bool (*IsGpuBlacklisted)(void);
};

typedef struct PPB_GpuBlacklist_Private_0_2 PPB_GpuBlacklist_Private;
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PPB_GPU_BLACKLIST_PRIVATE_H_ */

