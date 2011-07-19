/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PRIVATE_PPB_UMA_PRIVATE_H_
#define PPAPI_C_PRIVATE_PPB_UMA_PRIVATE_H_

#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_UMA_PRIVATE_INTERFACE "PPB_UMA(Private);0.1"

struct PPB_UMA_Private {
  /**
   * HistogramCustomTimes is a pointer to a function which records a time
   * sample given in milliseconds in the histogram given by |name|, possibly
   * creating the histogram if it does not exist.
   */
  void (*HistogramCustomTimes)(struct PP_Var name,
                               int64_t sample,
                               int64_t min, int64_t max,
                               uint32_t bucket_count);

  /**
   * HistogramCustomCounts is a pointer to a function which records a sample
   * in the histogram given by |name|, possibly creating the histogram if it
   * does not exist.
   */
  void (*HistogramCustomCounts)(struct PP_Var name,
                                int32_t sample,
                                int32_t min, int32_t max,
                                uint32_t bucket_count);

  /**
   * HistogramEnumeration is a pointer to a function which records a sample
   * in the histogram given by |name|, possibly creating the histogram if it
   * does not exist.  The sample represents a value in an enumeration bounded
   * by |boundary_value|, that is, sample < boundary_value always.
   */
  void (*HistogramEnumeration)(struct PP_Var name,
                               int32_t sample,
                               int32_t boundary_value);
};

#endif  /* PPAPI_C_PRIVATE_PPB_UMA_PRIVATE_H_ */
