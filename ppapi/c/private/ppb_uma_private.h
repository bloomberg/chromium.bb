/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/ppb_uma_private.idl modified Mon Nov 18 14:39:43 2013. */

#ifndef PPAPI_C_PRIVATE_PPB_UMA_PRIVATE_H_
#define PPAPI_C_PRIVATE_PPB_UMA_PRIVATE_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_UMA_PRIVATE_INTERFACE_0_2 "PPB_UMA_Private;0.2"
#define PPB_UMA_PRIVATE_INTERFACE PPB_UMA_PRIVATE_INTERFACE_0_2

/**
 * @file
 * This file defines the <code>PPB_UMA_Private</code> interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * Contains functions for plugins to report UMA usage stats.
 */
struct PPB_UMA_Private_0_2 {
  /**
   * HistogramCustomTimes is a pointer to a function which records a time
   * sample given in milliseconds in the histogram given by |name|, possibly
   * creating the histogram if it does not exist.
   */
  void (*HistogramCustomTimes)(PP_Instance instance,
                               struct PP_Var name,
                               int64_t sample,
                               int64_t min,
                               int64_t max,
                               uint32_t bucket_count);
  /**
   * HistogramCustomCounts is a pointer to a function which records a sample
   * in the histogram given by |name|, possibly creating the histogram if it
   * does not exist.
   */
  void (*HistogramCustomCounts)(PP_Instance instance,
                                struct PP_Var name,
                                int32_t sample,
                                int32_t min,
                                int32_t max,
                                uint32_t bucket_count);
  /**
   * HistogramEnumeration is a pointer to a function which records a sample
   * in the histogram given by |name|, possibly creating the histogram if it
   * does not exist.  The sample represents a value in an enumeration bounded
   * by |boundary_value|, that is, sample < boundary_value always.
   */
  void (*HistogramEnumeration)(PP_Instance instance,
                               struct PP_Var name,
                               int32_t sample,
                               int32_t boundary_value);
};

typedef struct PPB_UMA_Private_0_2 PPB_UMA_Private;
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PPB_UMA_PRIVATE_H_ */

