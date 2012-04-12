/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/ppp_flash_browser_operations.idl,
 *   modified Wed Apr 11 16:26:42 2012.
 */

#ifndef PPAPI_C_PRIVATE_PPP_FLASH_BROWSER_OPERATIONS_H_
#define PPAPI_C_PRIVATE_PPP_FLASH_BROWSER_OPERATIONS_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"

#define PPP_FLASH_BROWSEROPERATIONS_INTERFACE_1_0 \
    "PPP_Flash_BrowserOperations;1.0"
#define PPP_FLASH_BROWSEROPERATIONS_INTERFACE \
    PPP_FLASH_BROWSEROPERATIONS_INTERFACE_1_0

/**
 * @file
 * This file contains the <code>PPP_Flash_BrowserOperations</code> interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * This interface allows the browser to request the plugin do things.
 */
struct PPP_Flash_BrowserOperations_1_0 {
  /**
   * This function allows the plugin to implement the "Clear site data" feature.
   *
   * @plugin_data_path String containing the directory where the plugin data is
   * stored. On UTF16 systems (Windows), this will be encoded as UTF-8. It will
   * be an absolute path and will not have a directory separator (slash) at the
   * end.
   *
   * @arg site String specifying which site to clear the data for. This will
   * be null to clear data for all sites.
   *
   * @arg flags Currently always 0 in Chrome to clear all data. This may be
   * extended in the future to clear only specific types of data.
   *
   * @arg max_age The maximum age in seconds to clear data for. This allows the
   * plugin to implement "clear past hour" and "clear past data", etc.
   *
   * @return PP_TRUE on success, PP_FALSE on failure.
   *
   * See also the NPP_ClearSiteData function in NPAPI.
   * https://wiki.mozilla.org/NPAPI:ClearSiteData
   */
  PP_Bool (*ClearSiteData)(const char* plugin_data_path,
                           const char* site,
                           uint64_t flags,
                           uint64_t max_age);
};

typedef struct PPP_Flash_BrowserOperations_1_0 PPP_Flash_BrowserOperations;
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PPP_FLASH_BROWSER_OPERATIONS_H_ */

