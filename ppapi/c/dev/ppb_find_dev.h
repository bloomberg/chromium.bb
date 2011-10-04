/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_find_dev.idl modified Tue Oct  4 08:34:10 2011. */

#ifndef PPAPI_C_DEV_PPB_FIND_DEV_H_
#define PPAPI_C_DEV_PPB_FIND_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_FIND_DEV_INTERFACE_0_3 "PPB_Find(Dev);0.3"
#define PPB_FIND_DEV_INTERFACE PPB_FIND_DEV_INTERFACE_0_3

/**
 * @file
 * This file defines the <code>PPB_Find_Dev</code> interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_Find_Dev {
  /**
   * Updates the number of find results for the current search term.  If
   * there are no matches 0 should be passed in.  Only when the plugin has
   * finished searching should it pass in the final count with final_result set
   * to PP_TRUE.
   */
  void (*NumberOfFindResultsChanged)(PP_Instance instance,
                                     int32_t total,
                                     PP_Bool final_result);
  /**
   * Updates the index of the currently selected search item.
   */
  void (*SelectedFindResultChanged)(PP_Instance instance, int32_t index);
};
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_FIND_DEV_H_ */

