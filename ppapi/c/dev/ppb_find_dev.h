/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_find_dev.idl modified Thu Mar 13 11:05:53 2014. */

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
/* TODO(raymes): Make PPP/PPB_Find_Dev a private interface. It's only used by
 * PDF currently and it's restrictive in the way it can be used. */
struct PPB_Find_Dev_0_3 {
  /**
   * Sets the instance of this plugin as the mechanism that will be used to
   * handle find requests in the renderer. This will only succeed if the plugin
   * is embedded within the content of the top level frame. Note that this will
   * result in the renderer handing over all responsibility for doing find to
   * the plugin and content from the rest of the page will not be searched.
   *
   *
   * In the case that the plugin is loaded directly as the top level document,
   * this function does not need to be called. In that case the plugin is
   * assumed to handle find requests.
   *
   * There can only be one plugin which handles find requests. If a plugin calls
   * this while an existing plugin is registered, the existing plugin will be
   * de-registered and will no longer receive any requests.
   */
  void (*SetPluginToHandleFindRequests)(PP_Instance instance);
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

typedef struct PPB_Find_Dev_0_3 PPB_Find_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_FIND_DEV_H_ */

